/**
 * @file BuoyEnums.h
 * @brief Enumeration types for buoy states and navigation modes
 */

#ifndef BUOY_ENUMS_H
#define BUOY_ENUMS_H

#include <cstdint>

/**
 * @brief General buoy states
 */
enum tEtatsGeneral
{
    INIT = 0,
    READY,
    MAINTENANCE,
    HOME_DEFINITION, 
    NAV
};

/**
 * @brief Navigation states
 */
enum tEtatsNav
{
    NAV_NOTHING = 0, 
    NAV_HOME,    
    NAV_HOLD,
    NAV_STOP,
    NAV_BASIC,
    NAV_CAP,
    NAV_TARGET
};

#endif // BUOY_ENUMS_H
