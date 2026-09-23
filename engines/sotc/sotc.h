// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_ENGINE_H
#define SOTC_ENGINE_H
#include "engines/engine.h"
namespace Sotc {
class SotcEngine : public Engine {
public:
 SotcEngine(OSystem *system) : Engine(system) {}
 Common::Error run() override;
};
}
#endif
