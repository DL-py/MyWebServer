#ifndef GLOBAL_HPP
#define GLOBAL_HPP
#include "../config/config.hpp"
#include "../logs/run_log.hpp"
#include "../asset/asset.hpp"
#include "../auth/session_store.h"

class DbPool;

extern MyConfig globalCFG;
extern Logger logger;
extern Asset globalAsset;
extern DbPool* globalDbPool;
extern SessionStore globalSessionStore;
#endif