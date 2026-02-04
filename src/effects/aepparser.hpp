/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

/**
 * @brief Single parameter extracted from an AE effect.
 */
struct AEParam {
    QString id;          ///< e.g. "ADBE Optics Compensation-0001"
    QString displayName; ///< e.g. "Field Of View (FOV)"
    double value{0.0};   ///< First BE double from cdat chunk
};

/**
 * @brief One After Effects effect with its match-name and parameters.
 */
struct AEEffect {
    QString matchName; ///< e.g. "ADBE Optics Compensation"
    QVector<AEParam> params;
};

/**
 * @brief An Effect Parade — one per AE layer, contains a chain of effects.
 */
struct AEEffectParade {
    QVector<AEEffect> effects;
};

/**
 * @brief Parser for After Effects .aep (RIFX binary) files.
 *
 * Extracts effect chains (Effect Parades) from AEP project files
 * by scanning the RIFX container for known chunk patterns:
 *   tdmn — effect/param match-names
 *   tdsn — param display names
 *   cdat — param values (BE IEEE 754 doubles)
 *   "ADBE Group End" — effect boundary delimiter
 */
class AepParser
{
public:
    /**
     * @brief Parse an .aep file and return all Effect Parades found.
     * @param filePath Absolute path to the .aep file
     * @return Vector of AEEffectParade (one per layer that has effects)
     */
    static QVector<AEEffectParade> parse(const QString &filePath);

private:
    static QVector<AEEffectParade> findEffectParades(const QByteArray &data);
    static AEEffectParade extractParade(const QByteArray &data, qint64 start, qint64 end);
    static double readBEDouble(const QByteArray &data, qint64 offset);
    static qint32 readBE32(const QByteArray &data, qint64 offset);
    static QString readNullTermString(const QByteArray &data, qint64 offset, int maxLen);
};
