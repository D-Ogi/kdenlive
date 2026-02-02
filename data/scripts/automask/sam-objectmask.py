#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import os
# if using Apple MPS, fall back to CPU for unsupported ops
# os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
import numpy as np
import torch
import sys
import argparse
from PIL import Image

def process_list(list_string):
    array_data = np.fromstring(list_string, dtype=int, sep=',')
    return array_data

def process_csv(array_data, csv_string, resize):
    # Convert the CSV string back to a NumPy array
    vals_list = csv_string.split(';')
    for vals in vals_list:
        frame, csv_data = vals.split("=")
        np_array = np.fromstring(csv_data, dtype=int, sep=',')

        # Reshape the array if necessary (e.g., if it was a 2D array)
        if resize > 1:
            cols = int((np.shape(np_array)[0])/resize)
            np_array = np_array.reshape(cols, resize)
        array_data[int(frame)] = np_array

    #return array_data

if __name__ == "__main__":
    parser = argparse.ArgumentParser("SAM Object Mask Creator")
    parser.add_argument("-P", "--point_coordinates", help="Points coordinates with frame, like '0=200,250,300,255;100=10,50' for 2 points at frame 0 and one at frame 100")
    parser.add_argument("-F", "--preview_frame", help="The frame index for preview", default=-1)
    parser.add_argument("-L", "--labels", help="Points labels, 1 for include, 0 for exclude, like '0=1,0;100=1' for frame 0 and 100")
    parser.add_argument("-B", "--box_coordinates", help="Box coordinates with frame, like '0=10,20,150,255'")
    parser.add_argument("-I", "--inputFolder", help="folder where input jpg files are stored", default="/tmp/src-frames")
    parser.add_argument("-O", "--output", help="folder for rendered png image for preview of folder for rendering", default="/tmp/")
    parser.add_argument("-M", "--model", help="path for the model")
    parser.add_argument("-C", "--config", help="config for the model")
    parser.add_argument("-D", "--device", help="enforce a device: cuda, cpu")
    parser.add_argument("--color", help="mask color", default="255,100,100,180")
    parser.add_argument("--bordercolor", help="mask border color", default="255,100,100,100")
    parser.add_argument("--border", help="mask border width", default="0")
    parser.add_argument('--offload', help="offload memory to CPU", action='store_true')
    parser.add_argument("--preview-scale", help="downscale factor for preview inference (e.g. 0.5 = half resolution)", type=float, default=1.0)
    parser.add_argument("--overlay-mode", help="overlay visualization: 0=color overlay, 1=alpha boundary, 2=alpha channel", type=int, default=0)
    args = parser.parse_args()
    #if (args.point_coordinates is None or args.labels is None) and args.box_coordinates is None:
    #    config = vars(args)
    #    print(config)
    #    sys.exit()

    box = {}
    points = {}
    labels = {}
    mask_color = {}
    border_color = {}
    requestedDevice = "cpu"
    if args.point_coordinates is not None:
        process_csv(points, args.point_coordinates, 2)
        process_csv(labels, args.labels, 1)
    if args.box_coordinates is not None:
        process_csv(box, args.box_coordinates, 4)
    preview_frame = int(args.preview_frame)
    if args.output is not None:
        output_frame = args.output
    if args.inputFolder is not None:
        inputFolder = args.inputFolder
    if args.model is not None:
        modelFile = args.model
    if args.config is not None:
        configFile = args.config
    if args.device is not None:
        requestedDevice = args.device
    borders = int(args.border)
    mask_color = process_list(args.color)
    border_color = process_list(args.bordercolor)

# select the device for computation
if requestedDevice is not None:
    device = torch.device(requestedDevice)
    #if requestedDevice.startswith("cuda"):
        #print(f"Using CUDA version: {torch.version.cuda}")
elif torch.cuda.is_available():
    device = torch.device("cuda")
    #print(f"Using CUDA version: {torch.version.cuda}")
elif torch.backends.mps.is_available():
    device = torch.device("mps")
else:
    device = torch.device("cpu")

if device.type == "cuda":
    # Check available memory
    memInfo = torch.cuda.mem_get_info()
    print(f"GPU MEMINFO: {memInfo[0]} - {memInfo[1]}", file=sys.stdout, flush=True)
    # use bfloat16 for the entire notebook
    torch.autocast("cuda", dtype=torch.bfloat16).__enter__()
    # turn on tfloat32 for Ampere GPUs (https://pytorch.org/docs/stable/notes/cuda.html#tensorfloat-32-tf32-on-ampere-devices)
    if torch.cuda.get_device_properties(0).major >= 8:
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True
elif device.type == "mps":
    print(
        "\nSupport for MPS devices is preliminary. SAM 2 is trained with CUDA and might "
        "give numerically different outputs and sometimes degraded performance on MPS. "
        "See e.g. https://github.com/pytorch/pytorch/issues/84936 for a discussion."
    )

from sam2.build_sam import build_sam2
from kdenlive_build_sam import build_sam2_video_predictor
from sam2.sam2_image_predictor import SAM2ImagePredictor

scriptFolder = os.path.dirname(os.path.abspath(__file__))
sam2_checkpoint = modelFile
model_cfg = configFile

sam2_model = build_sam2(model_cfg, sam2_checkpoint, device=device)
predictor = SAM2ImagePredictor(sam2_model)

overlay_mode = 0  # 0=color overlay, 1=alpha boundary, 2=alpha channel

def save_mask(mask, filename, obj_id=None):
    h, w = mask.shape[-2:]

    if overlay_mode == 2:
        # Alpha Channel mode: mask=1 → white opaque, mask=0 → transparent black
        mask_uint8 = (mask.reshape(h, w) * 255).astype(np.uint8)
        mask_image = np.stack([mask_uint8, mask_uint8, mask_uint8, mask_uint8], axis=-1)
    elif overlay_mode == 1:
        # Alpha Boundary mode: contour outline only, no fill
        import cv2
        mask_uint8 = mask.reshape(h, w).astype(np.uint8)
        contours = cv2.findContours(mask_uint8, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]
        mask_image = np.zeros((h, w, 4), dtype=np.uint8)
        border_w = max(borders, 2)  # Minimum 2px border for visibility
        cv2.drawContours(mask_image, contours, -1, mask_color.tolist(), border_w)
    else:
        # Color Overlay mode (default): colored semi-transparent fill
        mask_image = mask.reshape(h, w, 1) * mask_color.reshape(1, 1, -1)
        if borders > 0:
            import cv2
            mask_uint8 = mask.reshape(h, w).astype(np.uint8)
            contours = cv2.findContours(mask_uint8, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]
            mask_image = cv2.drawContours(mask_image.astype(np.uint8), contours, -1, border_color.tolist(), borders)

    pil_img = Image.fromarray(np.uint8(mask_image))
    pil_img.save(filename)


def show_points(coords, labels, ax, marker_size=200):
    pos_points = coords[labels == 1]
    neg_points = coords[labels == 0]
    ax.scatter(pos_points[:, 0], pos_points[:, 1], color='green', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)
    ax.scatter(neg_points[:, 0], neg_points[:, 1], color='red', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)

# scan all the JPEG frame names in this directory
frame_names = [
    p for p in os.listdir(inputFolder)
    if os.path.splitext(p)[-1] in [".jpg"]
]
frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))

preview_scale = 1.0

def generate_preview():
    global predictor, preview_scale
    if predictor is None:
        predictor = SAM2ImagePredictor(sam2_model)

    image = Image.open(os.path.join(inputFolder, frame_names[preview_frame]))
    orig_size = image.size  # (width, height)
    image = np.array(image.convert("RGB"))

    # Optionally downscale for faster CPU inference
    scaled_points = points.get(preview_frame) if points else None
    scaled_box = box.get(preview_frame) if box else None
    if preview_scale < 1.0 and preview_scale > 0:
        import cv2
        new_w = int(image.shape[1] * preview_scale)
        new_h = int(image.shape[0] * preview_scale)
        image = cv2.resize(image, (new_w, new_h), interpolation=cv2.INTER_AREA)
        if scaled_points is not None:
            scaled_points = (scaled_points * preview_scale).astype(np.float32)
        if scaled_box is not None:
            scaled_box = (scaled_box * preview_scale).astype(np.float32)

    predictor.set_image(image)
    masks, scores, logits = predictor.predict(
        point_coords=None if scaled_points is None else scaled_points,
        point_labels=None if not labels else labels[preview_frame],
        box=None if scaled_box is None else scaled_box,
        multimask_output=False)

    if len(masks) == 0:
        # No object detected — output blank transparent mask
        h, w = image.shape[:2]
        if preview_scale < 1.0 and preview_scale > 0:
            w, h = orig_size[0], orig_size[1]
        mask = np.zeros((h, w), dtype=bool)
    else:
        mask = masks[0]
    # Upscale mask back to original resolution if downscaled
    if preview_scale < 1.0 and preview_scale > 0:
        import cv2
        mask = cv2.resize(mask.astype(np.uint8), orig_size, interpolation=cv2.INTER_NEAREST).astype(bool)

    filename = output_frame + '/preview-{:05d}'.format(preview_frame) + '.png'
    save_mask(mask, filename, ann_obj_id)
    print(f"preview ok {preview_frame}", file=sys.stdout, flush=True)

def render_video():
    # Stream propagation: propagate and export each frame immediately
    # This gives per-frame progress feedback to the C++ side
    framesCount = len(frame_names)
    print("INFO:Propagating in video\n", file=sys.stdout, flush=True)
    processed = 0
    for out_frame_idx, out_obj_ids, out_mask_logits in videoPredictor.propagate_in_video(inference_state):
        # Export this frame immediately instead of collecting all first
        for i, out_obj_id in enumerate(out_obj_ids):
            out_mask = (out_mask_logits[i] > 0.0).cpu().numpy()
            filename = output_frame + '/{:05d}'.format(out_frame_idx) + '.png'
            save_mask(out_mask, filename, obj_id=out_obj_id)
        processed += 1
        print(f"frame_done {processed}/{framesCount}", file=sys.stdout, flush=True)

# take a look the first video frame
#frame_idx = 0
#plt.figure(figsize=(9, 6))
#plt.title(f"frame {frame_idx}")
#plt.imshow(Image.open(os.path.join(video_dir, frame_names[frame_idx])))
videoPredictor_initialized = False
ann_obj_id = 1  # give a unique id to each object we interact with (it can be any integers)

if device.type == "cuda" and torch.cuda.get_device_properties(0).major >= 8:
    videoPredictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)  #, vos_optimized=True)
else:
    videoPredictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)

while 1:
    raw_line = sys.stdin.readline()
    if not raw_line:
        # stdin closed (parent process died), exit cleanly
        sys.exit(0)
    line = raw_line.rstrip()
    if line.startswith("edit="):
        inArgs = parser.parse_args(line[5:].split())
        borders = int(inArgs.border)
        mask_color = process_list(inArgs.color)
        border_color = process_list(inArgs.bordercolor)
        continue

    if line.startswith("preview="):
        # Generate image preview
        inArgs = parser.parse_args(line[8:].split())
        if inArgs.point_coordinates is not None:
            process_csv(points, inArgs.point_coordinates, 2)
            process_csv(labels, inArgs.labels, 1)
        if inArgs.box_coordinates is not None:
            process_csv(box, inArgs.box_coordinates, 4)
        preview_frame = int(inArgs.preview_frame)
        borders = int(inArgs.border)
        mask_color = process_list(inArgs.color)
        border_color = process_list(inArgs.bordercolor)
        preview_scale = inArgs.preview_scale
        overlay_mode = inArgs.overlay_mode
        generate_preview()
        # get ready for rendering
        if videoPredictor_initialized == False:
            if args.offload:
                print("Offloading video to CPU\n", file=sys.stdout, flush=True)
            inference_state = videoPredictor.init_state(video_path=inputFolder, offload_video_to_cpu=args.offload)
            videoPredictor_initialized = True
        continue

    if line.startswith("render="):
        if videoPredictor_initialized == False:
            print("INFO:Still loading frames\n", file=sys.stdout, flush=True)
            continue
        # Final masks always use color overlay mode regardless of preview setting
        overlay_mode = 0
        # Destroy image predictor on first render
        if predictor is not None:
            del predictor
            predictor = None
        # Generate output frames
        output_frame = line[7:].rstrip()
        # Reset state for clean propagation (prevents duplicate keyframes on repeated render=)
        videoPredictor.reset_state(inference_state)
        first_list = list(points.keys())
        in_first = set(first_list)
        in_second = set(box.keys())
        in_second_but_not_in_first = in_second - in_first
        result = first_list + list(in_second_but_not_in_first)

        for frame in result:
            _, _, out_mask_logits = videoPredictor.add_new_points_or_box(
                inference_state=inference_state,
                frame_idx=frame,
                obj_id=ann_obj_id,
                box=None if not box else box[frame],
                points=None if not points else points[frame],
                labels=None if not labels else labels[frame]
            )
        render_video()
        print("mask ok", file=sys.stdout, flush=True)
        continue

    if line.startswith("rerender="):
        # Re-propagation after corrections: reset state and re-render with updated keyframes
        if videoPredictor_initialized == False:
            print("INFO:Still loading frames\n", file=sys.stdout, flush=True)
            continue
        # Final masks always use color overlay mode regardless of preview setting
        overlay_mode = 0
        output_frame = line[9:].rstrip()
        # Reset the predictor state for fresh propagation
        videoPredictor.reset_state(inference_state)
        # Re-add all keyframes (corrections may have been added via preview= commands)
        first_list = list(points.keys())
        in_first = set(first_list)
        in_second = set(box.keys())
        in_second_but_not_in_first = in_second - in_first
        result = first_list + list(in_second_but_not_in_first)

        for frame in result:
            _, _, out_mask_logits = videoPredictor.add_new_points_or_box(
                inference_state=inference_state,
                frame_idx=frame,
                obj_id=ann_obj_id,
                box=None if not box else box[frame],
                points=None if not points else points[frame],
                labels=None if not labels else labels[frame]
            )
        render_video()
        print("mask ok", file=sys.stdout, flush=True)
        continue

    if line == "finish":
        # Clean exit after all rendering is done
        del videoPredictor
        videoPredictor_initialized = False
        print("CLOSING...\n", file=sys.stdout, flush=True)
        sys.exit()

    if line == "q":
        print("CLOSING...\n", file=sys.stdout, flush=True)
        sys.exit()


#with torch.inference_mode(), torch.autocast("cuda", dtype=torch.bfloat16):


# Let's add a positive click at (x, y) = (210, 350) to get started
#points = np.array([[423, 556], [250, 220]], dtype=np.float32)
# for labels, `1` means positive click and `0` means negative click
#labels = np.array([1, 1], np.int32)



# show the results on the current (interacted) frame
# plt.figure(figsize=(9, 6))
# plt.title(f"frame {frame}")
# plt.imshow(Image.open(os.path.join(inputFolder, frame_names[frame])))
#show_points(points, labels, plt.gca())

    #plt.show()

# Transform output png into video with alpha:
# ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv
