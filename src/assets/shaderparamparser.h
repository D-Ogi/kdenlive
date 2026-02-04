/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDomElement>
#include <QString>
#include <QVector>

/**
 * @brief Parsed representation of a single //!PARAM block in an mpv .hook shader.
 *
 * mpv/libplacebo shader format:
 * @code
 *   //!PARAM ray_intensity
 *   //!DESC God ray intensity (0 = off)
 *   //!TYPE DYNAMIC float
 *   //!MINIMUM 0.0
 *   //!MAXIMUM 1.0
 *   0.0
 * @endcode
 */
struct ShaderParamInfo
{
    QString name;          ///< from //!PARAM <name>
    QString description;   ///< from //!DESC (optional)
    QString glslType;      ///< "float", "int", or "uint" from //!TYPE [DYNAMIC] <type>
    QString group;         ///< auto-extracted from name prefix (bloom_intensity → "Bloom")
    bool isDynamic{false}; ///< true if DYNAMIC keyword present in //!TYPE
    double minimum{0.0};
    double maximum{1.0};
    double defaultValue{0.0}; ///< first non-directive, non-empty line after //!PARAM
};

/**
 * @class ShaderParamParser
 * @brief Parses //!PARAM blocks from mpv/libplacebo .hook shader files.
 *
 * Each parameter block starts with `//!PARAM <name>` and is followed by
 * optional directives (//!DESC, //!TYPE, //!MINIMUM, //!MAXIMUM) until
 * the first non-`//!` non-empty line, which is the default value.
 */
class ShaderParamParser
{
public:
    /** @brief Parse all //!PARAM blocks from shader source text */
    static QVector<ShaderParamInfo> parse(const QString &shaderText);

    /** @brief Parse all //!PARAM blocks from a shader file on disk */
    static QVector<ShaderParamInfo> parseFile(const QString &filePath);

    /**
     * @brief Strip all //!PARAM blocks from shader text.
     *
     * Removes //!PARAM lines and their associated directives (//!DESC, //!TYPE,
     * //!MINIMUM, //!MAXIMUM) plus the bare default-value line, so the result
     * is valid for libplacebo which does not understand //!PARAM.
     */
    static QString strip(const QString &shaderText);

    /**
     * @brief Convert a ShaderParamInfo into a <parameter> QDomElement
     *        suitable for injection into an effect XML document.
     *
     * Produces XML like:
     * @code
     *   <parameter type="double" name="shader_param.ray_intensity"
     *              default="0.0" min="0.0" max="1.0" decimals="3">
     *       <name>ray_intensity</name>
     *       <comment>God ray intensity (0 = off)</comment>
     *   </parameter>
     * @endcode
     */
    static QDomElement toParameterElement(QDomDocument &doc, const ShaderParamInfo &param);
};
