#include "StyleTokens.h"

#include <Qt>

namespace {
QColor hex(const char *value)
{
    return QColor(QString::fromLatin1(value));
}
}  // namespace

#define STYLE_COLOR(name, value) \
    QColor StyleTokens::name() const \
    { \
        static const QColor color = hex(value); \
        return color; \
    }

#define STYLE_GLOBAL_COLOR(name, value) \
    QColor StyleTokens::name() const \
    { \
        return value; \
    }

StyleTokens::StyleTokens(QObject *parent)
    : QObject(parent)
{
}

STYLE_GLOBAL_COLOR(transparent, Qt::transparent)
STYLE_GLOBAL_COLOR(black, Qt::black)
STYLE_GLOBAL_COLOR(white, Qt::white)
STYLE_GLOBAL_COLOR(panel, Qt::black)
STYLE_COLOR(module, "#1c1c1e")
STYLE_COLOR(moduleHover, "#232326")
STYLE_COLOR(track, "#2c2c2e")
STYLE_COLOR(textPrimary, "#f5f5f7")
STYLE_COLOR(textSecondary, "#8e8e93")
STYLE_COLOR(textMuted, "#9b9da4")
STYLE_COLOR(accent, "#0a84ff")
STYLE_COLOR(success, "#34c759")
STYLE_COLOR(warning, "#ffcc00")
STYLE_COLOR(danger, "#ff3b30")
STYLE_COLOR(overviewCard, "#ee17181b")
STYLE_COLOR(overviewBorder, "#33ffffff")

int StyleTokens::radiusCapsule() const { return 999; }
int StyleTokens::radiusPanel() const { return 32; }
int StyleTokens::radiusModule() const { return 22; }

#undef STYLE_COLOR
#undef STYLE_GLOBAL_COLOR
