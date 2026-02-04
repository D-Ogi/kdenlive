/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "aepparser.hpp"

#include <QFile>
#include <QtEndian>
#include <cstring>

// Chunk FourCC constants
static const QByteArray RIFX_MAGIC = QByteArrayLiteral("RIFX");
static const QByteArray EGG_MAGIC = QByteArrayLiteral("Egg!");
static const QByteArray TDMN_TAG = QByteArrayLiteral("tdmn");
static const QByteArray TDSN_TAG = QByteArrayLiteral("tdsn");
static const QByteArray CDAT_TAG = QByteArrayLiteral("cdat");
static const QByteArray LIST_TAG = QByteArrayLiteral("LIST");
static const QByteArray TDGP_TAG = QByteArrayLiteral("tdgp");
static const QByteArray PARADE_MARKER = QByteArrayLiteral("ADBE Effect Parade");
static const QByteArray GROUP_END = QByteArrayLiteral("ADBE Group End");

double AepParser::readBEDouble(const QByteArray &data, qint64 offset)
{
    if (offset + 8 > data.size()) {
        return 0.0;
    }
    quint64 raw = qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(data.constData() + offset));
    double result;
    std::memcpy(&result, &raw, sizeof(double));
    return result;
}

qint32 AepParser::readBE32(const QByteArray &data, qint64 offset)
{
    if (offset + 4 > data.size()) {
        return 0;
    }
    return qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

QString AepParser::readNullTermString(const QByteArray &data, qint64 offset, int maxLen)
{
    // AEP uses zero-terminated strings inside tdmn/tdsn chunks
    // Read until null or maxLen
    QByteArray raw;
    for (int i = 0; i < maxLen && (offset + i) < data.size(); ++i) {
        char c = data.at(offset + i);
        if (c == '\0') break;
        raw.append(c);
    }
    return QString::fromUtf8(raw).trimmed();
}

QVector<AEEffectParade> AepParser::parse(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("AepParser: Cannot open file %s", qPrintable(filePath));
        return {};
    }

    QByteArray data = file.readAll();
    file.close();

    // Verify RIFX header
    if (data.size() < 12) {
        qWarning("AepParser: File too small");
        return {};
    }
    if (data.mid(0, 4) != RIFX_MAGIC) {
        qWarning("AepParser: Not a RIFX file");
        return {};
    }
    if (data.mid(8, 4) != EGG_MAGIC) {
        qWarning("AepParser: Not an AEP file (missing Egg! FourCC)");
        return {};
    }

    return findEffectParades(data);
}

QVector<AEEffectParade> AepParser::findEffectParades(const QByteArray &data)
{
    QVector<AEEffectParade> parades;

    // Scan for "ADBE Effect Parade" markers, then find the associated LIST tdgp
    qint64 searchPos = 0;
    while (searchPos < data.size()) {
        qint64 paradePos = data.indexOf(PARADE_MARKER, searchPos);
        if (paradePos < 0) break;

        // After the parade marker, look for the next LIST tdgp that contains the effect data
        // The tdgp LIST follows relatively close after the parade marker
        qint64 tdgpSearch = paradePos + PARADE_MARKER.size();
        qint64 tdgpStart = -1;
        qint64 tdgpEnd = -1;

        // Search forward for LIST + tdgp within a reasonable range (64KB)
        qint64 maxSearch = qMin(tdgpSearch + 65536, static_cast<qint64>(data.size()) - 8);
        for (qint64 i = tdgpSearch; i < maxSearch; ++i) {
            if (data.mid(i, 4) == LIST_TAG) {
                qint32 listSize = readBE32(data, i + 4);
                if (i + 8 < data.size() && data.mid(i + 8, 4) == TDGP_TAG) {
                    tdgpStart = i + 12; // Past LIST + size + tdgp
                    tdgpEnd = i + 8 + listSize;
                    if (tdgpEnd > data.size()) {
                        tdgpEnd = data.size();
                    }
                    break;
                }
            }
        }

        if (tdgpStart > 0 && tdgpEnd > tdgpStart) {
            AEEffectParade parade = extractParade(data, tdgpStart, tdgpEnd);
            if (!parade.effects.isEmpty()) {
                parades.append(parade);
            }
        }

        searchPos = paradePos + PARADE_MARKER.size();
    }

    return parades;
}

AEEffectParade AepParser::extractParade(const QByteArray &data, qint64 start, qint64 end)
{
    AEEffectParade parade;

    // State machine: scan tdmn/tdsn/cdat chunks linearly
    // tdmn with an effect matchName (no dash-number suffix) starts a new effect
    // tdmn with a param ID (has dash-number suffix like "-0001") starts a new param
    // tdsn gives the display name for the current param
    // cdat gives the value for the current param
    // "ADBE Group End" closes the current effect

    AEEffect currentEffect;
    AEParam currentParam;
    bool inEffect = false;
    bool inParam = false;

    qint64 pos = start;
    while (pos + 8 <= end) {
        QByteArray chunkTag = data.mid(pos, 4);
        qint32 chunkSize = readBE32(data, pos + 4);

        if (chunkSize < 0 || pos + 8 + chunkSize > end) {
            // Invalid chunk, try advancing
            pos += 4;
            continue;
        }

        qint64 chunkData = pos + 8;

        if (chunkTag == TDMN_TAG) {
            QString name = readNullTermString(data, chunkData, qMin(chunkSize, static_cast<qint32>(256)));

            if (name == QLatin1String("ADBE Group End")) {
                // Close current param if open
                if (inParam && inEffect) {
                    currentEffect.params.append(currentParam);
                    currentParam = AEParam();
                    inParam = false;
                }
                // Close current effect
                if (inEffect) {
                    parade.effects.append(currentEffect);
                    currentEffect = AEEffect();
                    inEffect = false;
                }
            } else if (name.startsWith(QLatin1String("ADBE")) || !name.isEmpty()) {
                // Check if this is a param ID (contains dash followed by digits)
                // e.g. "ADBE Optics Compensation-0001"
                int dashIdx = name.lastIndexOf(QLatin1Char('-'));
                bool isParamId = false;
                if (dashIdx > 0 && dashIdx < name.size() - 1) {
                    // Check if everything after the dash is digits
                    QStringView suffix = QStringView(name).mid(dashIdx + 1);
                    isParamId = true;
                    for (QChar c : suffix) {
                        if (!c.isDigit()) {
                            isParamId = false;
                            break;
                        }
                    }
                }

                if (isParamId) {
                    // Close previous param if open
                    if (inParam && inEffect) {
                        currentEffect.params.append(currentParam);
                    }
                    currentParam = AEParam();
                    currentParam.id = name;
                    inParam = true;
                } else if (name != QLatin1String("ADBE Effect Parade") &&
                           !name.startsWith(QLatin1String("ADBE Effect Built In Params")) &&
                           !name.startsWith(QLatin1String("ADBE Effect Parade"))) {
                    // New effect — close previous if open
                    if (inParam && inEffect) {
                        currentEffect.params.append(currentParam);
                        currentParam = AEParam();
                        inParam = false;
                    }
                    if (inEffect) {
                        parade.effects.append(currentEffect);
                    }
                    currentEffect = AEEffect();
                    currentEffect.matchName = name;
                    inEffect = true;
                    inParam = false;
                }
            }
        } else if (chunkTag == TDSN_TAG && inParam) {
            // Display name for current param — stored as a UTF-16BE or UTF-8 string
            // Skip first 4 bytes (string header: length or flags)
            if (chunkSize > 4) {
                currentParam.displayName = readNullTermString(data, chunkData + 4, qMin(chunkSize - 4, static_cast<qint32>(256)));
            }
        } else if (chunkTag == CDAT_TAG && inParam) {
            // First 8 bytes = BE double = parameter value
            if (chunkSize >= 8) {
                currentParam.value = readBEDouble(data, chunkData);
            }
        }

        // Advance to next chunk (chunks are 2-byte aligned in RIFX)
        qint64 advance = 8 + chunkSize;
        if (advance % 2 != 0) advance++;
        pos += advance;
    }

    // Close any open effect
    if (inParam && inEffect) {
        currentEffect.params.append(currentParam);
    }
    if (inEffect) {
        parade.effects.append(currentEffect);
    }

    return parade;
}
