#pragma once

#include <Arduino.h>

void initPeripherals();
void runMissionDashboard();
void runAuditReports();
void runGpsTools();
void runGpsStatus();
void runSdStatus();
void runSdFileBrowser();
void runBatteryStatus();
bool sdWriteTextFile(const char* path, const String& content);
