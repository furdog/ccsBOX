#pragma once
#include "LittleFS.h"

extern struct ccsbox ccsbox;

bool settings_committed;
bool filesystem_corrupted;

void ccsbox_fs_save_settings(struct ccsbox *self)
{
	printf("saving...\n");
	File f = LittleFS.open("/settings.bin", "w");

	if (!f) {
		printf("failed opening '/settings.bin' for write\n");
		return;
	}

	f.write((uint8_t *)&self->settings, sizeof(struct ccsbox_settings));
	f.close();
}

void ccsbox_fs_load_settings(struct ccsbox *self)
{
	/** First run. */
	if (!LittleFS.exists("/settings.bin"))
		ccsbox_fs_save_settings(self);

	printf("loading...\n");
	File f = LittleFS.open("/settings.bin", "r");
	if (!f) {
		printf("failed opening '/settings.bin' for read\n");
		return;
	}

	f.read((uint8_t *)&self->settings, sizeof(struct ccsbox_settings));

	f.close();
}

void ccsbox_fs_commit_settings(struct ccsbox *self)
{
	settings_committed = true;
}

bool ccsbox_fs_is_corrupted(struct ccsbox *self)
{
	return filesystem_corrupted;
}

void ccsbox_fs_init(struct ccsbox *self)
{
	settings_committed   = false;
	filesystem_corrupted = false;

	if (!LittleFS.begin(true)) {
		printf("failed to mount filesystem (corrupted)\n");
		filesystem_corrupted = true;
	}

	ccsbox_fs_load_settings(self);
}

void ccsbox_fs_update(struct ccsbox *self)
{
	if (filesystem_corrupted) {
		printf("failed to update filesystem (corrupted)\n");
		return;
	}

	if (settings_committed) {
		ccsbox_fs_save_settings(self);
		settings_committed = false;
	}
}
