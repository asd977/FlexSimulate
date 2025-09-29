#pragma once

#include <QString>

namespace StyleConstants
{
inline const QString& primaryButton()
{
    static const QString style = QStringLiteral(
        "QPushButton{background:#1f2937;color:#ffffff;padding:8px 20px;border-radius:8px;font-weight:600;}"
        "QPushButton:hover:!disabled{background:#111827;}"
        "QPushButton:pressed:!disabled{background:#0f172a;}"
        "QPushButton:disabled{background:#9ca3af;color:#f9fafb;}"
    );
    return style;
}

inline const QString& secondaryButton()
{
    static const QString style = QStringLiteral(
        "QPushButton{background:#ffffff;color:#1f2937;padding:8px 20px;border-radius:8px;border:1px solid #d1d5db;}"
        "QPushButton:hover:!disabled{background:#f3f4f6;}"
        "QPushButton:pressed:!disabled{background:#e5e7eb;}"
        "QPushButton:disabled{color:#9ca3af;border-color:#e5e7eb;}"
    );
    return style;
}

inline const QString& subtleCard()
{
    static const QString style = QStringLiteral(
        "background:#f8f9fb;border:1px solid #e5e7eb;border-radius:12px;"
    );
    return style;
}
}

