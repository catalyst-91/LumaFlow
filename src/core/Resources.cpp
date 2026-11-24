#include "Resources.h"
#include <QtGlobal>
#include <QDir>
static void initRes() { Q_INIT_RESOURCE(resources); }
namespace lf { void initResources() { static bool done = false; if (!done) { initRes(); done = true; } } }
