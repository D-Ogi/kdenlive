/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "aepmapper.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QtMath>

// UI-only AE effects that should be skipped
static const QSet<QString> s_uiOnlyEffects = {
    QStringLiteral("ADBE Slider Control"),
    QStringLiteral("ADBE Checkbox Control"),
    QStringLiteral("ADBE Color Control"),
    QStringLiteral("ADBE Point Control"),
    QStringLiteral("ADBE Angle Control"),
    QStringLiteral("ADBE Layer Control"),
    QStringLiteral("ADBE Dropdown Control"),
};

// Effects we can't map: curve/keyframe data, or MLT equivalent is excluded
static const QSet<QString> s_skipEffects = {
    QStringLiteral("ADBE CurvesCustom"),
    QStringLiteral("ADBE Shift Channels"), // avfilter.shuffleplanes is in excluded_effects.txt
    QStringLiteral("ADBE Geometry2"),       // qtblend as standalone effect causes black frames
};

const AEParam *AepMapper::findParam(const AEEffect &effect, const QString &idFragment)
{
    for (const AEParam &p : effect.params) {
        if (p.id.contains(idFragment)) {
            return &p;
        }
    }
    return nullptr;
}

double AepMapper::paramValue(const AEEffect &effect, const QString &idFragment, double defaultVal)
{
    const AEParam *p = findParam(effect, idFragment);
    return p ? p->value : defaultVal;
}

const AEParam *AepMapper::findParamByDisplayName(const AEEffect &effect, const QString &nameFragment)
{
    for (const AEParam &p : effect.params) {
        if (p.displayName.contains(nameFragment)) {
            return &p;
        }
    }
    return nullptr;
}

double AepMapper::paramValueByDisplayName(const AEEffect &effect, const QString &nameFragment, double defaultVal)
{
    const AEParam *p = findParamByDisplayName(effect, nameFragment);
    return p ? p->value : defaultVal;
}

double AepMapper::fovToDefish(double fovDegrees)
{
    // frei0r.defish0r "Amount" has factor="1000" in Kdenlive XML,
    // so the stored value is multiplied: frei0r_value = xml_value / 1000.
    // AE Optics Compensation FOV: 0-180 degrees → frei0r Amount 0-1
    // We store xml_value = (FOV/180) * 1000
    return qBound(0.0, fovDegrees / 180.0 * 1000.0, 1000.0);
}

int AepMapper::blurToBoxblur(double blurriness)
{
    // AE Gaussian Blur Blurriness -> avfilter.boxblur av.lr
    // Roughly 1:1 mapping, clamped
    return qBound(0, qRound(blurriness), 100);
}

QMap<QString, QString> AepMapper::glo2ToSoftglow(const AEEffect &effect)
{
    // AE Glo2 -> frei0r.softglow
    // Param suffixes: -0001=Blend Mode, -0002=Radius, -0003=Intensity, -0004=Threshold
    QMap<QString, QString> params;

    double blendMode = paramValue(effect, QStringLiteral("-0001"), 1.0);
    double glowRadius = paramValue(effect, QStringLiteral("-0002"), 50.0);
    double glowIntensity = paramValue(effect, QStringLiteral("-0003"), 50.0);
    double glowThreshold = paramValue(effect, QStringLiteral("-0004"), 50.0);

    // frei0r.softglow blurblend is a list: 0=Screen, 0.5=Overlay, 1.0=Add
    // AE Glo2 blend modes: 1=Screen(default), 2=Add, others→Screen
    QString blurblend = QStringLiteral("0"); // Screen (default, safest)
    if (qRound(blendMode) == 2) {
        blurblend = QStringLiteral("1.0"); // Add
    }
    params[QStringLiteral("blurblend")] = blurblend;

    // frei0r.softglow params are 0-1 (no factor)
    params[QStringLiteral("blur")] = QString::number(qBound(0.0, glowRadius / 100.0, 1.0), 'f', 2);
    params[QStringLiteral("brightness")] = QString::number(qBound(0.0, glowIntensity / 100.0, 1.0), 'f', 2);
    params[QStringLiteral("sharpness")] = QString::number(qBound(0.0, glowThreshold / 100.0, 1.0), 'f', 2);

    return params;
}

QMap<QString, QString> AepMapper::noiseToAvfilter(const AEEffect &effect)
{
    // AE Noise HLS2 -> avfilter.noise
    // Param suffix: -0001=Noise amount
    QMap<QString, QString> params;

    double noiseAmount = paramValue(effect, QStringLiteral("-0001"), 25.0);

    // avfilter.noise alls = noise strength (0-100)
    params[QStringLiteral("av.alls")] = QString::number(qBound(0, qRound(noiseAmount), 100));

    return params;
}

QMap<QString, QString> AepMapper::lumaKeyToMlt(const AEEffect &effect)
{
    // AE Luma Key -> MLT native lumakey (NOT avfilter.lumakey which is excluded)
    // Param suffix: -0001=Threshold
    QMap<QString, QString> params;

    double threshold = paramValue(effect, QStringLiteral("-0001"), 128.0);

    // MLT native lumakey: threshold is 0-255 (integer)
    params[QStringLiteral("threshold")] = QString::number(qBound(0, qRound(threshold), 255));

    return params;
}

AepMapper::MltMapping AepMapper::mapEffect(const AEEffect &aeEffect)
{
    MltMapping result;
    const QString &mn = aeEffect.matchName;

    qWarning() << "AEP MAPPER: effect matchName=" << mn;
    if (s_uiOnlyEffects.contains(mn) || s_skipEffects.contains(mn)) {
        qWarning() << "AEP MAPPER: skipping (ui-only or skip list)";
        result.supported = false;
        return result;
    }

    if (mn == QLatin1String("ADBE Optics Compensation")) {
        MltEffect eff;
        eff.mltEffectId = QStringLiteral("frei0r.defish0r");
        double fov = paramValue(aeEffect, QStringLiteral("-0001"), 45.0);
        // Amount: distortion strength (0-1)
        eff.params[QStringLiteral("Amount")] = QString::number(fovToDefish(fov), 'f', 3);
        // DeFish: 0 = apply distortion (rectilinear->fisheye), 1 = remove (default)
        eff.params[QStringLiteral("DeFish")] = QStringLiteral("0");
        // Type: 0 = Equidistant (matches AE barrel distortion model)
        eff.params[QStringLiteral("Type")] = QStringLiteral("0");
        result.effects.append(eff);
        result.supported = true;
    } else if (mn == QLatin1String("ADBE Gaussian Blur 2")) {
        MltEffect eff;
        eff.mltEffectId = QStringLiteral("avfilter.boxblur");
        double blur = paramValue(aeEffect, QStringLiteral("-0001"), 10.0);
        int radius = blurToBoxblur(blur);
        eff.params[QStringLiteral("av.lr")] = QString::number(radius);
        eff.params[QStringLiteral("av.lp")] = QStringLiteral("2");
        result.effects.append(eff);
        result.supported = true;
    } else if (mn == QLatin1String("ADBE Glo2")) {
        MltEffect eff;
        eff.mltEffectId = QStringLiteral("frei0r.softglow");
        eff.params = glo2ToSoftglow(aeEffect);
        result.effects.append(eff);
        result.supported = true;
    } else if (mn == QLatin1String("ADBE Noise HLS2")) {
        MltEffect eff;
        eff.mltEffectId = QStringLiteral("avfilter.noise");
        eff.params = noiseToAvfilter(aeEffect);
        result.effects.append(eff);
        result.supported = true;
    } else if (mn == QLatin1String("ADBE Lumetri")) {
        // Lumetri uses display-name lookup (nested groups have unstable indices)
        double exposure = paramValueByDisplayName(aeEffect, QStringLiteral("Exposure"), 0.0);
        double blacks = paramValueByDisplayName(aeEffect, QStringLiteral("Blacks"), 0.0);

        // Exposure -> avfilter.eq brightness (AE 0 = normal, +/-4 range -> MLT -1..1)
        if (qAbs(exposure) > 0.001) {
            MltEffect eqEff;
            eqEff.mltEffectId = QStringLiteral("avfilter.eq");
            eqEff.params[QStringLiteral("av.brightness")] = QString::number(exposure / 4.0, 'f', 3);
            result.effects.append(eqEff);
        }

        // Blacks -> avfilter.colorbalance shadow channels (uniform shift)
        if (qAbs(blacks) > 0.001) {
            MltEffect cbEff;
            cbEff.mltEffectId = QStringLiteral("avfilter.colorbalance");
            double val = blacks / 100.0;
            cbEff.params[QStringLiteral("av.rs")] = QString::number(val, 'f', 3);
            cbEff.params[QStringLiteral("av.gs")] = QString::number(val, 'f', 3);
            cbEff.params[QStringLiteral("av.bs")] = QString::number(val, 'f', 3);
            result.effects.append(cbEff);
        }

        result.supported = !result.effects.isEmpty();
    } else if (mn == QLatin1String("ADBE Luma Key")) {
        MltEffect eff;
        eff.mltEffectId = QStringLiteral("lumakey");
        eff.params = lumaKeyToMlt(aeEffect);
        result.effects.append(eff);
        result.supported = true;
    } else {
        qWarning() << "AEP MAPPER: UNMAPPED effect" << mn;
        result.supported = false;
    }

    return result;
}

QDomDocument AepMapper::toEffectGroupXml(const QVector<AEEffectParade> &parades, const QString &presetName)
{
    QDomDocument doc;
    if (parades.isEmpty()) {
        return doc;
    }

    // Merge unique effects from all parades
    // Use matchName as the dedup key
    QVector<AEEffect> mergedEffects;
    QSet<QString> seenEffects;

    // First, find the "best" parade (most mappable effects)
    int bestIdx = 0;
    int bestCount = 0;
    for (int i = 0; i < parades.size(); ++i) {
        int count = 0;
        for (const AEEffect &eff : parades[i].effects) {
            MltMapping m = mapEffect(eff);
            if (m.supported) count++;
        }
        if (count > bestCount) {
            bestCount = count;
            bestIdx = i;
        }
    }

    // Start with best parade's effects
    for (const AEEffect &eff : parades[bestIdx].effects) {
        if (!seenEffects.contains(eff.matchName)) {
            mergedEffects.append(eff);
            seenEffects.insert(eff.matchName);
        }
    }

    // Merge in unique effects from other parades
    for (int i = 0; i < parades.size(); ++i) {
        if (i == bestIdx) continue;
        for (const AEEffect &eff : parades[i].effects) {
            if (!seenEffects.contains(eff.matchName)) {
                mergedEffects.append(eff);
                seenEffects.insert(eff.matchName);
            }
        }
    }

    // Build XML
    QDomProcessingInstruction xmlDecl = doc.createProcessingInstruction(QStringLiteral("xml"),
                                                                        QStringLiteral("version=\"1.0\""));
    doc.appendChild(xmlDecl);

    // Sanitize preset name for use as XML id
    QString safeId = QStringLiteral("AE_") + presetName;
    safeId.replace(QLatin1Char(' '), QLatin1Char('_'));
    safeId.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QString());

    QDomElement root = doc.createElement(QStringLiteral("effectgroup"));
    root.setAttribute(QStringLiteral("id"), safeId);
    doc.appendChild(root);

    int mappedCount = 0;
    for (const AEEffect &aeEffect : mergedEffects) {
        MltMapping mapping = mapEffect(aeEffect);
        if (!mapping.supported) continue;

        for (const MltEffect &mltEff : mapping.effects) {
            QDomElement effectEl = doc.createElement(QStringLiteral("effect"));
            effectEl.setAttribute(QStringLiteral("id"), mltEff.mltEffectId);

            for (auto it = mltEff.params.constBegin(); it != mltEff.params.constEnd(); ++it) {
                QDomElement propEl = doc.createElement(QStringLiteral("property"));
                propEl.setAttribute(QStringLiteral("name"), it.key());
                propEl.appendChild(doc.createTextNode(it.value()));
                effectEl.appendChild(propEl);
            }

            root.appendChild(effectEl);
            mappedCount++;
        }
    }

    // Add name and description
    QDomElement nameEl = doc.createElement(QStringLiteral("name"));
    nameEl.appendChild(doc.createTextNode(presetName));
    root.appendChild(nameEl);

    QDomElement descEl = doc.createElement(QStringLiteral("description"));
    descEl.appendChild(doc.createTextNode(
        QStringLiteral("Imported from %1.aep (%2 effects mapped)")
            .arg(presetName)
            .arg(mappedCount)));
    root.appendChild(descEl);

    if (mappedCount == 0) {
        return QDomDocument(); // Return empty doc if nothing was mapped
    }

    return doc;
}
