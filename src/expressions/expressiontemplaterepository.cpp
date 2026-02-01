/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressiontemplaterepository.h"
#include "doc/kdenlivedoc.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

ExpressionTemplateRepository &ExpressionTemplateRepository::instance()
{
    static ExpressionTemplateRepository repo;
    return repo;
}

ExpressionTemplateRepository::ExpressionTemplateRepository()
    : QObject(nullptr)
{
    loadDefaultTemplates();
    loadUserTemplates();
}

// ── Default templates (mirrored from old buildTemplatePanel) ──────────

void ExpressionTemplateRepository::loadDefaultTemplates()
{
    struct Def
    {
        const char *seed;
        const char *name;
        const char *category;
        const char *expression;
        const char *description;
    };

    static const Def defs[] = {
        // Fade & Transition
        {"fade-in", "Fade In", "Fade & Transition", "linear(time, 0, duration, 0, value)", "Fade from 0 to full value over the entire clip"},
        {"fade-out", "Fade Out", "Fade & Transition", "linear(time, 0, duration, value, 0)", "Fade from full value to 0 over the entire clip"},
        {"fade-in-out", "Fade In/Out", "Fade & Transition",
         "time < duration * 0.2\n"
         "  ? ease(time, 0, duration * 0.2, 0, value)\n"
         "  : time > duration * 0.8\n"
         "    ? ease(time, duration * 0.8, duration, value, 0)\n"
         "    : value",
         "Fade in over first 20%, hold, fade out over last 20%"},
        {"smooth-fade-in", "Smooth Fade In (ease)", "Fade & Transition", "ease(time, 0, duration, 0, value)", "Ease-in from 0 to full value (smooth start)"},

        // Oscillation & Motion
        {"gentle-shake", "Gentle Shake", "Oscillation & Motion", "wiggle(3, 0.05)", "Subtle random motion, 3 Hz"},
        {"camera-shake", "Camera Shake", "Oscillation & Motion", "wiggle(8, 0.15)", "Energetic shake, 8 Hz"},
        {"smooth-sine-wave", "Smooth Sine Wave", "Oscillation & Motion", "value + 0.15 * Math.sin(time * Math.PI * 2)",
         "Sinusoidal oscillation around base value"},
        {"breathing", "Breathing", "Oscillation & Motion", "value + 0.08 * Math.sin(time * Math.PI * 0.8)", "Slow sinusoidal pulse (inhale/exhale rhythm)"},
        {"pendulum", "Pendulum", "Oscillation & Motion",
         "value + 0.2 * Math.sin(time * Math.PI * 4)\n"
         "  * Math.max(0, 1 - time / duration)",
         "Damped oscillation, slowing over clip duration"},

        // Audio-Reactive
        {"audio-pulse", "Audio Pulse", "Audio-Reactive", "linear(audioLevel(\"Both\", time), 0.05, 0.8, value * 0.3, value)",
         "Value pulses with audio level (quiet=30%, loud=full)"},
        {"audio-invert", "Audio Invert", "Audio-Reactive", "linear(audioLevel(\"Both\", time), 0.05, 0.8, value, value * 0.2)",
         "High value when quiet, low when loud"},
        {"audio-shake", "Audio Shake", "Audio-Reactive", "wiggle(6, audioLevel(\"Both\", time) * 0.3)", "Shake intensity driven by audio level"},
        {"audio-rms-smooth", "Audio RMS Smooth", "Audio-Reactive", "linear(audioRms(\"Both\", time, 0.1), 0.02, 0.5, value * 0.2, value)",
         "Smooth audio-reactive using RMS (100ms window, less jittery than peak)"},

        // Stepped & Quantized
        {"strobe", "Strobe (2 fps)", "Stepped & Quantized", "posterizeTime(2);\nlinear(time, 0, duration, 0, value)",
         "Hold each value for 0.5 seconds (posterized time)"},
        {"random-per-second", "Random per Second", "Stepped & Quantized", "posterizeTime(1);\nrandom(0, value)", "New random value each second"},
        {"step-grow", "Step Grow (5 steps)", "Stepped & Quantized", "Math.floor(time / duration * 5) / 5 * value",
         "Increase in 5 equal steps across clip duration"},

        // Path / Mask (for Rotoscoping effect spline parameter)
        {"path-rotating-gear", "Rotating Gear", "Path / Mask",
         "var n = 20;\n"
         "var cx = 0.5, cy = 0.5;\n"
         "var r1 = 0.15, r2 = 0.25;\n"
         "var angle = time * 2 * Math.PI / 4;\n"
         "var pts = [];\n"
         "for (var i = 0; i < n * 2; i++) {\n"
         "    var a = angle + i * Math.PI / n;\n"
         "    var r = (i % 2 === 0) ? r2 : r1;\n"
         "    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);\n"
         "}\n"
         "createPath(pts, [], [], true)",
         "Gear-shaped mask rotating once every 4 seconds"},
        {"path-pulsing-star", "Pulsing Star (Audio)", "Path / Mask",
         "var n = 5;\n"
         "var cx = 0.5, cy = 0.5;\n"
         "var level = audioLevel(\"Both\", time);\n"
         "var r1 = 0.1 + level * 0.1;\n"
         "var r2 = 0.2 + level * 0.15;\n"
         "var pts = [];\n"
         "for (var i = 0; i < n * 2; i++) {\n"
         "    var a = i * Math.PI / n - Math.PI / 2;\n"
         "    var r = (i % 2 === 0) ? r2 : r1;\n"
         "    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);\n"
         "}\n"
         "createPath(pts, [], [], true)",
         "5-point star mask that pulses with audio level"},
        {"path-horizontal-wipe", "Horizontal Wipe", "Path / Mask",
         "var progress = time / duration;\n"
         "createPath(\n"
         "    [[0,0], [progress,0], [progress,1], [0,1]],\n"
         "    [], [], true\n"
         ")",
         "Left-to-right wipe revealing over clip duration"},
        {"path-iris-circle", "Iris Circle", "Path / Mask",
         "var n = 32;\n"
         "var cx = 0.5, cy = 0.5;\n"
         "var maxR = 0.5;\n"
         "var r = maxR * time / duration;\n"
         "var pts = [];\n"
         "for (var i = 0; i < n; i++) {\n"
         "    var a = i * 2 * Math.PI / n;\n"
         "    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);\n"
         "}\n"
         "createPath(pts, [], [], true)",
         "Circular iris opening from center over clip duration"},

        // Keyframe & Dynamics
        {"kf-overshoot", "Overshoot", "Keyframe & Dynamics",
         "var freq = 4;\nvar decay = 6;\nvar n = nearestKey(time);\nif (time > n.time) {\n    var dt = time - n.time;\n    var amp = velocityAtTime(n.time) / "
         "(Math.PI * 2 * freq);\n    value + amp * Math.sin(dt * Math.PI * 2 * freq) * Math.exp(-decay * dt);\n} else {\n    value;\n}",
         "Damped sine overshoot after each keyframe — classic springy settling"},
        {"kf-bounce", "Bounce", "Keyframe & Dynamics",
         "var freq = 5;\nvar decay = 5;\nvar n = nearestKey(time);\nif (time > n.time) {\n    var dt = time - n.time;\n    var amp = velocityAtTime(n.time) / "
         "(Math.PI * 2 * freq);\n    value + Math.abs(amp * Math.sin(dt * Math.PI * 2 * freq)) * Math.exp(-decay * dt);\n} else {\n    value;\n}",
         "Always-positive bounce using absolute sine — object lands and bounces"},
        {"kf-elastic", "Elastic Snap", "Keyframe & Dynamics",
         "var freq = 3;\nvar decay = 4;\nvar n = nearestKey(time);\nif (time > n.time) {\n    var dt = time - n.time;\n    var vel = velocityAtTime(n.time);\n "
         "   var amp = vel * 0.03;\n    value + amp * Math.sin(dt * Math.PI * 2 * freq) * Math.exp(-decay * dt);\n} else {\n    value;\n}",
         "Elastic snap with amplitude scaled by velocity — fast moves ring more"},
        {"kf-inertia", "Inertia", "Keyframe & Dynamics",
         "if (numKeys > 0 && time > key(numKeys).time) {\n    var lastKey = key(numKeys);\n    var vel = velocityAtTime(lastKey.time);\n    var dt = time - "
         "lastKey.time;\n    lastKey.value + vel * dt;\n} else {\n    value;\n}",
         "Continue at last keyframe's velocity — smooth drift after animation ends"},

        // Showcase — advanced multi-system expressions
        {"showcase-cinematic-swell", "Cinematic Audio Swell", "Showcase",
         "// Smooth audio envelope with attack/release shaping.\n"
         "// Combines RMS smoothing, non-linear response curve,\n"
         "// and a timeline-position fade to avoid popping at cut points.\n"
         "var raw = audioRms(\"Both\", time, 0.15);\n"
         "// Apply logarithmic response curve (perceived loudness)\n"
         "var shaped = Math.pow(raw, 0.6);\n"
         "// Smooth with 2-frame temporal average for cinematic feel\n"
         "var prev = audioRms(\"Both\", time - 1/fps, 0.15);\n"
         "var smoothed = shaped * 0.7 + Math.pow(prev, 0.6) * 0.3;\n"
         "// Fade in/out at clip edges (1s each) to avoid hard cuts\n"
         "var edgeFade = Math.min(\n"
         "    ease(time, 0, 1, 0, 1),\n"
         "    ease(time, duration - 1, duration, 1, 0)\n"
         ");\n"
         "// Map to parameter range: quiet = 20% of value, loud = 110%\n"
         "linear(smoothed * edgeFade, 0, 0.8, value * 0.2, value * 1.1)",
         "Audio-reactive parameter with logarithmic loudness curve, "
         "temporal smoothing, and automatic edge fading. Ideal for "
         "bloom, glow, or opacity effects on music videos."},

        {"showcase-marker-hit-flash", "Marker Hit Flash", "Showcase",
         "// Flash to peak on each timeline marker, then decay.\n"
         "// Place guides at beat hits — the expression does the rest.\n"
         "// Works with any number of markers, no keyframes needed.\n"
         "var peak = value;         // flash target\n"
         "var rest = value * 0.15;  // resting level between hits\n"
         "var decay = 8;            // exponential decay speed\n"
         "var result = rest;\n"
         "// Find the most recent marker before current time\n"
         "for (var i = marker.numKeys; i >= 1; i--) {\n"
         "    var m = marker.key(i);\n"
         "    if (m.time <= time) {\n"
         "        var dt = time - m.time;\n"
         "        // Sharp attack (instant), exponential release\n"
         "        var flash = peak * Math.exp(-decay * dt);\n"
         "        result = Math.max(rest, flash);\n"
         "        break;\n"
         "    }\n"
         "}\n"
         "// Also check if a marker is coming soon — subtle anticipation ramp\n"
         "for (var j = 1; j <= marker.numKeys; j++) {\n"
         "    var upcoming = marker.key(j);\n"
         "    var until = upcoming.time - time;\n"
         "    if (until > 0 && until < 0.15) {\n"
         "        // Gentle 150ms anticipation ramp\n"
         "        result = Math.max(result, linear(until, 0.15, 0, rest, rest + (peak - rest) * 0.2));\n"
         "        break;\n"
         "    }\n"
         "}\n"
         "result",
         "Exponential flash-and-decay triggered by timeline markers/guides. "
         "Includes subtle anticipation ramp before each hit. Place guides on "
         "beat drops and apply to brightness, scale, or glow intensity."},

        {"showcase-cascade-stagger", "Cascade Stagger", "Showcase",
         "// Staggered animation across clips on the same track.\n"
         "// Each clip's animation starts with a delay based on its\n"
         "// position, creating a domino/cascade effect.\n"
         "// Uses cross-clip references to calculate total sequence span.\n"
         "var clipCount = index + 1;  // clips seen so far\n"
         "// Per-clip delay: 0.3s stagger between each clip's animation start\n"
         "var stagger = 0.3;\n"
         "var localDelay = index * stagger;\n"
         "// Animation duration per clip (excluding stagger wait)\n"
         "var animDur = Math.min(1.5, duration - localDelay);\n"
         "if (animDur <= 0) { value; }\n"
         "else {\n"
         "    // Local time shifted by stagger\n"
         "    var t = time - localDelay;\n"
         "    if (t < 0) {\n"
         "        0;  // waiting for cascade to reach this clip\n"
         "    } else if (t < animDur) {\n"
         "        // Ease in with overshoot (elastic settle)\n"
         "        var progress = t / animDur;\n"
         "        var elastic = 1 - Math.pow(1 - progress, 3) *\n"
         "            Math.cos(progress * Math.PI * 3);\n"
         "        elastic * value;\n"
         "    } else {\n"
         "        // Sustain + subtle breathing after settle\n"
         "        value + 0.02 * Math.sin((t - animDur) * Math.PI * 1.5);\n"
         "    }\n"
         "}",
         "Domino-style staggered animation across clips on a track. "
         "Each clip starts its ease-in 0.3s after the previous one, "
         "with elastic overshoot settling. Apply to opacity or scale "
         "for a reveal cascade effect."},
    };

    for (const auto &d : defs) {
        ExpressionTemplate tmpl;
        tmpl.id = ExpressionTemplate::stableId(QString::fromLatin1(d.seed));
        tmpl.name = QString::fromLatin1(d.name);
        tmpl.category = QString::fromLatin1(d.category);
        tmpl.expression = QString::fromLatin1(d.expression);
        tmpl.description = QString::fromLatin1(d.description);
        tmpl.tier = TemplateTier::Default;
        m_defaults.insert(tmpl.id, tmpl);
    }
}

// ── User templates (JSON files on disk) ───────────────────────────────

QString ExpressionTemplateRepository::userTemplatesDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/expression_templates");
}

void ExpressionTemplateRepository::loadUserTemplates()
{
    QDir dir(userTemplatesDir());
    if (!dir.exists()) return;

    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &fileName : files) {
        QFile file(dir.absoluteFilePath(fileName));
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            ExpressionTemplate tmpl = ExpressionTemplate::fromJson(doc.object(), TemplateTier::User);
            if (!tmpl.id.isEmpty()) {
                m_userTemplates.insert(tmpl.id, tmpl);
            }
        }
    }
}

void ExpressionTemplateRepository::saveUserTemplate(const ExpressionTemplate &tmpl)
{
    QDir dir(userTemplatesDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QFile file(dir.absoluteFilePath(tmpl.id + QStringLiteral(".json")));
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray data = QJsonDocument(tmpl.toJson()).toJson(QJsonDocument::Indented);
        if (file.write(data) != data.size()) {
            qWarning() << "ExpressionTemplateRepository: failed to write template" << tmpl.id << file.errorString();
        }
    } else {
        qWarning() << "ExpressionTemplateRepository: cannot open template file for writing" << file.fileName() << file.errorString();
    }
}

void ExpressionTemplateRepository::deleteUserTemplateFile(const QString &id)
{
    QDir dir(userTemplatesDir());
    QFile::remove(dir.absoluteFilePath(id + QStringLiteral(".json")));
}

// ── Query ─────────────────────────────────────────────────────────────

QVector<ExpressionTemplate> ExpressionTemplateRepository::allTemplates(TemplateTier tier) const
{
    switch (tier) {
    case TemplateTier::Default:
        return m_defaults.values().toVector();
    case TemplateTier::User:
        return m_userTemplates.values().toVector();
    case TemplateTier::Project:
        return m_projectTemplates.values().toVector();
    }
    return {};
}

QVector<ExpressionTemplate> ExpressionTemplateRepository::allTemplates() const
{
    QVector<ExpressionTemplate> result;
    result.reserve(m_defaults.size() + m_userTemplates.size() + m_projectTemplates.size());
    result.append(m_defaults.values().toVector());
    result.append(m_userTemplates.values().toVector());
    result.append(m_projectTemplates.values().toVector());
    return result;
}

ExpressionTemplate ExpressionTemplateRepository::getTemplate(const QString &id) const
{
    if (m_projectTemplates.contains(id)) return m_projectTemplates.value(id);
    if (m_userTemplates.contains(id)) return m_userTemplates.value(id);
    if (m_defaults.contains(id)) return m_defaults.value(id);
    return {};
}

bool ExpressionTemplateRepository::hasTemplate(const QString &id) const
{
    return m_defaults.contains(id) || m_userTemplates.contains(id) || m_projectTemplates.contains(id);
}

QStringList ExpressionTemplateRepository::categories(TemplateTier tier) const
{
    QSet<QString> cats;
    const auto &map = (tier == TemplateTier::Default) ? m_defaults : (tier == TemplateTier::User) ? m_userTemplates : m_projectTemplates;
    for (const auto &tmpl : map) {
        if (!tmpl.category.isEmpty()) {
            cats.insert(tmpl.category);
        }
    }
    QStringList result = cats.values();
    result.sort();
    return result;
}

// ── CRUD ──────────────────────────────────────────────────────────────

QString ExpressionTemplateRepository::addTemplate(ExpressionTemplate tmpl)
{
    if (tmpl.id.isEmpty()) {
        tmpl.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (tmpl.tier == TemplateTier::User) {
        m_userTemplates.insert(tmpl.id, tmpl);
        saveUserTemplate(tmpl);
    } else if (tmpl.tier == TemplateTier::Project) {
        m_projectTemplates.insert(tmpl.id, tmpl);
    }
    Q_EMIT templateAdded(tmpl.id);
    return tmpl.id;
}

void ExpressionTemplateRepository::updateTemplate(const ExpressionTemplate &tmpl)
{
    if (tmpl.tier == TemplateTier::Default) return; // read-only
    if (tmpl.tier == TemplateTier::User) {
        m_userTemplates[tmpl.id] = tmpl;
        saveUserTemplate(tmpl);
    } else if (tmpl.tier == TemplateTier::Project) {
        m_projectTemplates[tmpl.id] = tmpl;
    }
    Q_EMIT templateUpdated(tmpl.id);
}

void ExpressionTemplateRepository::removeTemplate(const QString &id)
{
    if (m_defaults.contains(id)) return; // can't remove defaults

    // Auto-detach linked instances before removal
    if (m_projectTemplates.contains(id)) {
        const auto &tmpl = m_projectTemplates[id];
        for (const auto &link : tmpl.linkedInstances) {
            Q_EMIT linkedExpressionChanged(link.effectId, link.paramName, QString());
        }
        m_projectTemplates.remove(id);
    } else if (m_userTemplates.contains(id)) {
        m_userTemplates.remove(id);
        deleteUserTemplateFile(id);
    }
    Q_EMIT templateRemoved(id);
}

// ── Linking ───────────────────────────────────────────────────────────

void ExpressionTemplateRepository::linkInstance(const QString &templateId, const QString &effectId, const QString &paramName)
{
    if (!m_projectTemplates.contains(templateId)) return;
    auto &tmpl = m_projectTemplates[templateId];
    // Avoid duplicates
    for (const auto &link : std::as_const(tmpl.linkedInstances)) {
        if (link.effectId == effectId && link.paramName == paramName) return;
    }
    tmpl.linkedInstances.append({effectId, paramName});
}

void ExpressionTemplateRepository::unlinkInstance(const QString &templateId, const QString &effectId, const QString &paramName)
{
    if (!m_projectTemplates.contains(templateId)) return;
    auto &instances = m_projectTemplates[templateId].linkedInstances;
    for (int i = 0; i < instances.size(); ++i) {
        if (instances[i].effectId == effectId && instances[i].paramName == paramName) {
            instances.removeAt(i);
            return;
        }
    }
}

void ExpressionTemplateRepository::propagateTemplateChange(const QString &templateId)
{
    if (!m_projectTemplates.contains(templateId)) return;
    const auto &tmpl = m_projectTemplates[templateId];
    for (const auto &link : tmpl.linkedInstances) {
        Q_EMIT linkedExpressionChanged(link.effectId, link.paramName, tmpl.expression);
    }
}

// ── Import / Export ───────────────────────────────────────────────────

bool ExpressionTemplateRepository::importFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    ExpressionTemplate tmpl = ExpressionTemplate::fromJson(doc.object(), TemplateTier::User);
    // Assign new UUID on import to avoid collisions
    tmpl.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tmpl.tier = TemplateTier::User;
    tmpl.linkedInstances.clear();
    addTemplate(tmpl);
    return true;
}

bool ExpressionTemplateRepository::exportToFile(const QString &templateId, const QString &filePath)
{
    ExpressionTemplate tmpl = getTemplate(templateId);
    if (tmpl.id.isEmpty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    // Export without linked instances (they're project-specific)
    ExpressionTemplate exportCopy = tmpl;
    exportCopy.linkedInstances.clear();
    file.write(QJsonDocument(exportCopy.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

// ── Project lifecycle ─────────────────────────────────────────────────

void ExpressionTemplateRepository::loadProjectTemplates(KdenliveDoc *doc)
{
    if (!doc) return;
    clearProjectTemplates();

    const QString json = doc->getDocumentProperty(QStringLiteral("expressionTemplates"));
    if (json.isEmpty()) return;

    QJsonDocument jdoc = QJsonDocument::fromJson(json.toUtf8());
    if (!jdoc.isArray()) return;

    const QJsonArray arr = jdoc.array();
    for (const auto &val : arr) {
        ExpressionTemplate tmpl = ExpressionTemplate::fromJson(val.toObject(), TemplateTier::Project);
        if (!tmpl.id.isEmpty()) {
            m_projectTemplates.insert(tmpl.id, tmpl);
        }
    }
}

void ExpressionTemplateRepository::saveProjectTemplates(KdenliveDoc *doc)
{
    if (!doc) return;
    if (m_projectTemplates.isEmpty()) {
        doc->setDocumentProperty(QStringLiteral("expressionTemplates"), QString());
        return;
    }

    QJsonArray arr;
    for (const auto &tmpl : std::as_const(m_projectTemplates)) {
        arr.append(tmpl.toJson());
    }
    doc->setDocumentProperty(QStringLiteral("expressionTemplates"), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ExpressionTemplateRepository::clearProjectTemplates()
{
    m_projectTemplates.clear();
}

QString ExpressionTemplateRepository::promoteToProject(const QString &sourceId)
{
    ExpressionTemplate source = getTemplate(sourceId);
    if (source.id.isEmpty()) return {};

    ExpressionTemplate promoted;
    promoted.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    promoted.name = source.name;
    promoted.category = source.category;
    promoted.expression = source.expression;
    promoted.description = source.description;
    promoted.tier = TemplateTier::Project;

    m_projectTemplates.insert(promoted.id, promoted);
    Q_EMIT templateAdded(promoted.id);
    return promoted.id;
}
