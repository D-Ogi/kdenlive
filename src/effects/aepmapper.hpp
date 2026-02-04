/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "aepparser.hpp"

#include <QDomDocument>
#include <QMap>
#include <QString>
#include <QVector>

/**
 * @brief Maps After Effects effects to MLT equivalents and generates
 * Kdenlive effect-group XML templates.
 */
class AepMapper
{
public:
    /// A single MLT effect with its parameter values.
    struct MltEffect {
        QString mltEffectId;
        QMap<QString, QString> params; ///< MLT param name -> value
    };

    /**
     * @brief Result of mapping one AE effect.
     *
     * A single AE effect can map to one or more MLT effects
     * (e.g. Lumetri -> avfilter.eq + avfilter.colorbalance).
     */
    struct MltMapping {
        QVector<MltEffect> effects;
        bool supported{false};
    };

    /**
     * @brief Convert parsed AE effect parades to a Kdenlive <effectgroup> XML.
     *
     * Picks the "best" parade (most mappable effects), or merges unique effects
     * from all parades if the file contains multiple layers.
     *
     * @param parades All parades from the AEP file
     * @param presetName Human-readable name (used as effectgroup id)
     * @return QDomDocument with the effectgroup XML, or empty doc on failure
     */
    static QDomDocument toEffectGroupXml(const QVector<AEEffectParade> &parades, const QString &presetName);

    /**
     * @brief Map a single AE effect to its MLT equivalent(s).
     */
    static MltMapping mapEffect(const AEEffect &aeEffect);

private:
    /// FOV in degrees -> frei0r.defish0r "Amount" parameter (0-1 range)
    static double fovToDefish(double fovDegrees);

    /// AE Gaussian Blur "Blurriness" -> avfilter.boxblur av.lr
    static int blurToBoxblur(double blurriness);

    /// AE Glo2 -> frei0r.softglow params
    static QMap<QString, QString> glo2ToSoftglow(const AEEffect &effect);

    /// AE Noise HLS2 -> avfilter.noise params
    static QMap<QString, QString> noiseToAvfilter(const AEEffect &effect);

    /// AE Luma Key -> lumakey params
    static QMap<QString, QString> lumaKeyToMlt(const AEEffect &effect);

    /// Find a parameter by partial ID match within an AE effect
    static const AEParam *findParam(const AEEffect &effect, const QString &idFragment);
    static double paramValue(const AEEffect &effect, const QString &idFragment, double defaultVal = 0.0);

    /// Find a parameter by display-name substring (for effects with unstable indices like Lumetri)
    static const AEParam *findParamByDisplayName(const AEEffect &effect, const QString &nameFragment);
    static double paramValueByDisplayName(const AEEffect &effect, const QString &nameFragment, double defaultVal = 0.0);
};
