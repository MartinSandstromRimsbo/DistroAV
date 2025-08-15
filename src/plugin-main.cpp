/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
	Copyright (C) 2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#include "plugin-main.h"

#include "obs-support/obs-app.hpp"

#include <obs-module.h>
#include <obs.h>

#include <QDir>
#include <QLibrary>
#include <QRegularExpression>
#include <QMessageBox>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>

OBS_DECLARE_MODULE()

// Stub function for missing obs_module_text
extern "C" {
    const char *obs_module_text(const char *lookup_string) {
        // Simple stub - just return the lookup string as-is
        return lookup_string;
    }
}

// These files are compiled separately by CMake

// NDI library handle
QLibrary *loaded_lib = nullptr;
const NDIlib_v6 *ndiLib = nullptr;

// Plugin info
#define PLUGIN_DISPLAY_NAME "MixStage NDI"
#define PLUGIN_MIN_NDI_VERSION "6.0.0"
// PLUGIN_REDIRECT_NDI_REDIST_URL already defined elsewhere

// Forward declarations
extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

// ndi-filter removed due to frontend API dependencies

extern struct obs_source_info create_alpha_filter_info();
struct obs_source_info alpha_filter_info;

// Load NDI library function
const NDIlib_v6 *load_ndilib();
typedef const NDIlib_v6 *(*NDIlib_v6_load_)(void);

// Missing functions that other files expect
QString rehostUrl(const char *url)
{
	return QString::fromUtf8(url);
}

// Config class is already defined in config.h
// Implement the missing static methods
Config* Config::_instance = nullptr;

void Config::Initialize() {
	if (!_instance) {
		_instance = new Config();
	}
}

Config* Config::Current(bool load) {
	(void)load; // Suppress unused parameter warning
	if (!_instance) {
		Initialize();
	}
	return _instance;
}

void Config::Destroy() {
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}

Config::Config() {
	// Set default values
	OutputEnabled = false;
	OutputName = "MixStage NDI Output";
	OutputGroups = "";
	PreviewOutputEnabled = false;
	PreviewOutputName = "MixStage NDI Preview";
	PreviewOutputGroups = "";
	TallyProgramEnabled = false;
	TallyPreviewEnabled = false;
}

// Version comparison function
bool is_version_supported(const char *version, const char *min_version)
{
	QStringList version_parts = QString(version).split('.');
	QStringList min_version_parts = QString(min_version).split('.');

	int max_parts = qMax(version_parts.size(), min_version_parts.size());

	for (int i = 0; i < max_parts; i++) {
		int version_part = (i < version_parts.size()) ? version_parts[i].toInt() : 0;
		int min_version_part = (i < min_version_parts.size()) ? min_version_parts[i].toInt() : 0;

		if (version_part > min_version_part)
			return true;
		if (version_part < min_version_part)
			return false;
	}

	return true;
}

bool obs_module_load(void)
{
	obs_log(LOG_DEBUG, "+obs_module_load()");

	// Load NDI library
    ndiLib = load_ndilib();
    if (!ndiLib) {
        obs_log(LOG_ERROR, "ERR-401 - NDI library failed to load. Please install NDI Runtime >= 6.0.0");
        // Friendly prompt: offer to open official download, or continue without NDI
        const QString title = QString::fromUtf8("NDI Runtime not found");
        const QString text = QString::fromUtf8(
            "NDI Runtime (v6+) was not found on this system.\n\n"
            "- Install to enable NDI Sources/Outputs\n"
            "- Or continue without NDI")
                ;
        QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton);
        QPushButton *installBtn = box.addButton(QString::fromUtf8("Open NDI Download"), QMessageBox::AcceptRole);
        box.addButton(QString::fromUtf8("Continue without NDI"), QMessageBox::RejectRole);
        box.setDefaultButton(installBtn);
        box.exec();
        if (box.clickedButton() == installBtn) {
            QDesktopServices::openUrl(QUrl(QString::fromUtf8(NDI_OFFICIAL_WEB_URL)));
        }
        // Skip loading the plugin (app continues to run without NDI)
        return false;
    }

	// Initialize NDI
	auto initialized = ndiLib->initialize();
	if (!initialized) {
		obs_log(LOG_ERROR, "ERR-406 - NDI library could not initialize due to unsupported CPU.");
		return false;
	}

	obs_log(LOG_INFO, "obs_module_load: NDI library detected ('%s')", ndiLib->version());

	// Check NDI version
	QString ndi_version_short =
		QRegularExpression(R"((\d+\.\d+(\.\d+)?(\.\d+)?$))").match(ndiLib->version()).captured(1);
	obs_log(LOG_INFO, "NDI Version detected: %s", QT_TO_UTF8(ndi_version_short));

	if (!is_version_supported(QT_TO_UTF8(ndi_version_short), PLUGIN_MIN_NDI_VERSION)) {
		obs_log(LOG_ERROR,
			"ERR-425 - %s requires at least NDI version %s. NDI Version detected: %s. Plugin will unload.",
			PLUGIN_DISPLAY_NAME, PLUGIN_MIN_NDI_VERSION, QT_TO_UTF8(ndi_version_short));
		return false;
	}

	obs_log(LOG_INFO, "obs_module_load: NDI library initialized successfully");

	// Register NDI sources and outputs
	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);

	// ndi-filter registration removed due to frontend API dependencies

	alpha_filter_info = create_alpha_filter_info();
	obs_register_source(&alpha_filter_info);

	obs_log(LOG_INFO, "obs_module_load: DistroAV plugin loaded successfully");
	obs_log(LOG_DEBUG, "-obs_module_load()");

	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_DEBUG, "+obs_module_unload()");

	if (ndiLib) {
		ndiLib->destroy();
		ndiLib = nullptr;
	}

	if (loaded_lib) {
		delete loaded_lib;
	}

	obs_log(LOG_DEBUG, "-obs_module_unload(): goodbye!");
}

const NDIlib_v6 *load_ndilib()
{
    auto locations = QStringList();

    // 0) Prefer the app's bundled Frameworks so Finder launches work when bundled
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString frameworksDir = QDir::cleanPath(appDir + "/../Frameworks");
    locations << frameworksDir;

    // 1) Honor NDI v6 official runtime env var
    {
        const QByteArray v6 = qgetenv("NDI_RUNTIME_DIR_V6");
        if (!v6.isEmpty()) {
            locations << QString::fromUtf8(v6);
        }
    }

    // 2) Legacy env var used by some setups
    {
        const QByteArray redist = qgetenv("NDILIB_REDIST_FOLDER");
        if (!redist.isEmpty()) {
            locations << QString::fromUtf8(redist);
        }
    }
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	// Linux, MacOS
	// https://github.com/DistroAV/DistroAV/blob/master/lib/ndi/NDI%20SDK%20Documentation.pdf
	// "6.1 LOCATING THE LIBRARY
	// ... the redistributable on MacOS is installed within `/usr/local/lib` ..."
	// Flatpak install will look for the NDI lib in /app/plugins/DistroAV/extra/lib
	locations << "/usr/lib";
	locations << "/usr/local/lib";
#if defined(Q_OS_LINUX)
	locations << "/app/plugins/DistroAV/extra/lib";
#endif
#endif
    auto lib_path = QString();
#if defined(Q_OS_LINUX)
	// Linux
	auto regex = QRegularExpression("libndi\\.so\\.(\\d+)");
	int max_version = 0;
#endif
    // Well-known macOS install locations for the official NDI SDK
    locations << "/Library/NDI SDK for Apple/lib";
    locations << "/Library/NDI/lib";
    locations << "/opt/homebrew/opt/libndi/lib"; // Homebrew fallback

    for (const auto &location : locations) {
		auto dir = QDir(location);
#if defined(Q_OS_LINUX)
		// Linux
		auto filters = QStringList("libndi.so.*");
		dir.setNameFilters(filters);
		auto file_names = dir.entryList(QDir::Files);
		for (const auto &file_name : file_names) {
			auto match = regex.match(file_name);
			if (match.hasMatch()) {
				int version = match.captured(1).toInt();
				if (version > max_version) {
					max_version = version;
					lib_path = dir.absoluteFilePath(file_name);
				}
			}
		}
#else
		// MacOS, Windows
        auto temp_path = QDir::cleanPath(dir.absoluteFilePath(NDILIB_LIBRARY_NAME));
		obs_log(LOG_DEBUG, "load_ndilib: Trying '%s'", QT_TO_UTF8(QDir::toNativeSeparators(temp_path)));
		auto file_info = QFileInfo(temp_path);
		if (file_info.exists() && file_info.isFile()) {
			lib_path = temp_path;
			break;
		}
#endif
	}
	if (!lib_path.isEmpty()) {
		obs_log(LOG_DEBUG, "load_ndilib: Found '%s'; attempting to load NDI library...",
			QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
		loaded_lib = new QLibrary(lib_path, nullptr);
		if (loaded_lib->load()) {
			obs_log(LOG_DEBUG, "load_ndilib: NDI library loaded successfully");
			NDIlib_v6_load_ lib_load =
				reinterpret_cast<NDIlib_v6_load_>(loaded_lib->resolve("NDIlib_v6_load"));
			if (lib_load != nullptr) {
				obs_log(LOG_DEBUG, "load_ndilib: NDIlib_v6_load found");
				return lib_load();
			} else {
				obs_log(LOG_ERROR, "ERR-405 - Error loading the NDI Library from path: '%s'",
					QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
				obs_log(LOG_DEBUG, "load_ndilib: ERROR: NDIlib_v6_load not found in loaded library");
			}
		} else {
			obs_log(LOG_ERROR, "ERR-402 - Error loading QLibrary with error: '%s'",
				QT_TO_UTF8(loaded_lib->errorString()));
			obs_log(LOG_DEBUG, "load_ndilib: ERROR: QLibrary returned the following error: '%s'",
				QT_TO_UTF8(loaded_lib->errorString()));
			delete loaded_lib;
			loaded_lib = nullptr;
		}
	}

	obs_log(LOG_ERROR,
		"ERR-404 - NDI library not found, DistroAV cannot continue. Read the wiki and install the NDI Libraries.");
	obs_log(LOG_DEBUG, "load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
