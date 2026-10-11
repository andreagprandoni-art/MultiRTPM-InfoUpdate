/*
Multistream Info Panel
Pannello per aggiornare titolo, categoria e info della live su Twitch, Kick e YouTube da dentro OBS.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/bmem.h>

#include <QString>

#include "panel.hpp"
#include "services.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static const char *kDockId = "multistream-info-panel";
static bool g_dockAdded = false;

bool obs_module_load(void)
{
	char *dir = obs_module_config_path("");
	svc::setConfigDir(QString::fromUtf8(dir ? dir : ""));
	bfree(dir);

	auto *panel = new InfoPanel();
	if (obs_frontend_add_dock_by_id(kDockId, "Info Live", panel)) {
		g_dockAdded = true;
	} else {
		obs_log(LOG_WARNING, "impossibile aggiungere il dock 'Info Live'");
		delete panel;
	}

	obs_log(LOG_INFO, "plugin caricato (versione %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	if (g_dockAdded)
		obs_frontend_remove_dock(kDockId);
	obs_log(LOG_INFO, "plugin scaricato");
}
