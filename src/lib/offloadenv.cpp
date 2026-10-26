// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/offloadenv.h"

#include "agino-gpu/configgen.h"

#include <QFile>

namespace AginoGpu {

QStringList offloadEnvironment(const Gpu &dgpu, int nvidiaCount, const QString &vulkanDeviceName)
{
    QStringList env;
    if (dgpu.vendor == Vendor::Nvidia) {
        env << QStringLiteral("__NV_PRIME_RENDER_OFFLOAD=1");
        if (nvidiaCount > 1) {
            // TODO(multi-NVIDIA): the provider index should be the position of `dgpu` among the
            // NVIDIA GPUs; the signature carries no such index yet, so index 0 is used.
            env << QStringLiteral("__NV_PRIME_RENDER_OFFLOAD_PROVIDER=NVIDIA-G0");
        }
        env << QStringLiteral("__GLX_VENDOR_LIBRARY_NAME=nvidia")
            << QStringLiteral("__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_nvidia.json")
            << QStringLiteral("__VK_LAYER_NV_optimus=NVIDIA_only")
            // data/xsession/68agino-gpu limits Qt's FFmpeg backend to VA-API for the session
            // (its CUDA probe loads libcuda and wakes the GPU); an app on the GPU gets CUDA back.
            << QStringLiteral("QT_FFMPEG_DECODING_HW_DEVICE_TYPES=cuda,vaapi")
            << QStringLiteral("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES=cuda,vaapi");
        return env;
    }

    QString pci = dgpu.pciAddress;
    pci.replace(QLatin1Char(':'), QLatin1Char('_')).replace(QLatin1Char('.'), QLatin1Char('_'));
    env << QStringLiteral("DRI_PRIME=pci-%1").arg(pci)
        << QStringLiteral("MESA_VK_DEVICE_SELECT=%1:%2")
               .arg(dgpu.vendorId, 4, 16, QLatin1Char('0'))
               .arg(dgpu.deviceId, 4, 16, QLatin1Char('0'));
    if (!vulkanDeviceName.isEmpty()) {
        env << QStringLiteral("DXVK_FILTER_DEVICE_NAME=%1").arg(vulkanDeviceName)
            << QStringLiteral("VKD3D_FILTER_DEVICE_NAME=%1").arg(vulkanDeviceName);
    }
    return env;
}

QByteArray offloadEnvFileContents(const QStringList &environment)
{
    QByteArray out = generatedHeader();
    for (const QString &line : environment)
        out += line.toUtf8() + '\n';
    return out;
}

QStringList parseOffloadEnvFile(const QByteArray &contents)
{
    QStringList out;
    const QList<QByteArray> lines = contents.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        out << line;
    }
    return out;
}

QStringList readOffloadEnv(const Root &root)
{
    QFile f(root.path(QLatin1String(kOffloadEnvPath)));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return parseOffloadEnvFile(f.readAll());
}

} // namespace AginoGpu
