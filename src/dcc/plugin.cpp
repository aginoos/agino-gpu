// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpupage.h"

#include "dccfactory.h"

// DCC_FACTORY_CLASS builds a class name from its argument, so it needs an unqualified one.
using GpuPageModel = AginoGpu::GpuPage;

DCC_FACTORY_CLASS(GpuPageModel)

#include "plugin.moc"
