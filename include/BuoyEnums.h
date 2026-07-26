/**
 * @file BuoyEnums.h
 * @brief Enumeration types for buoy states and navigation modes
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef BUOY_ENUMS_H
#define BUOY_ENUMS_H

#include <cstdint>

/**
 * @brief General buoy states
 */
enum tEtatsGeneral
{
    IDENTIFICATION = 0,  ///< Doit rester aligné avec l'enum de la bouée (ModeManagement.h)
    INIT,
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
