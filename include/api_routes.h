#pragma once

#include <ESPAsyncWebServer.h>

// Registers every HTTP endpoint the device exposes. Call once from setup(),
// before server.begin().
void registerRoutes(AsyncWebServer& server);
