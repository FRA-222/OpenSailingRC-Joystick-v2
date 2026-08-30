/**
 * @file Logger.cpp
 * @brief Implémentation du gestionnaire centralisé de logs
 * @author Philippe Hubert
 * @date 2025
 * 
 * Implémentation des méthodes de la classe Logger pour la gestion
 * centralisée des logs sur port série et écran M5Stack Atom S3.
 * 
 * Cette version est adaptée pour le M5Stack Atom S3 avec son écran 128x128 pixels.
 * 
 * @copyright Copyright (c) 2025
 * @version 1.0
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "Logger.h"
#include <stdarg.h>

// Initialisation des variables statiques
int Logger::nbLineDisplayed = 0;
bool Logger::enableSerialOutput = true;
bool Logger::enableLcdOutput = false;  // Désactivé par défaut (petit écran)
int Logger::textSize = 1;

/**
 * @brief Initialise le système de logging
 * 
 * @param serialEnabled Active ou désactive la sortie sur le port série
 * @param lcdEnabled Active ou désactive la sortie sur l'écran LCD
 */
void Logger::init(bool serialEnabled, bool lcdEnabled) {
    enableSerialOutput = serialEnabled;
    enableLcdOutput = lcdEnabled;
    nbLineDisplayed = 0;
    textSize = 1;

    if (enableSerialOutput) {
        Serial.begin(115200);
        delay(50);  // Attente stabilisation UART
    }

    if (enableLcdOutput) {
        M5.Display.clear(TFT_BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextSize(textSize);
    }
}

/**
 * @brief Enregistre un message sur les sorties configurées
 * 
 * Cette méthode gère l'enregistrement des informations en :
 * - Imprimant sur le Moniteur Série (Serial) si activé
 * - Gérant l'affichage LCD avec effacement automatique si activé
 * 
 * @param message Message sous forme de chaîne de caractères à enregistrer
 * 
 * @note L'écran LCD est effacé et le curseur réinitialisé après MAX_LCD_LINES lignes
 * @note Utilise Serial pour la sortie série sur Core2
 */
void Logger::log(const String& message) {
    // Sortie sur le port série si activée
    if (enableSerialOutput) {
        Serial.println(message);
        Serial.flush();
    }
    
    // Sortie sur l'écran LCD si activée
    if (enableLcdOutput) {
        // Efface l'afficheur tous les MAX_LCD_LINES lignes
        nbLineDisplayed++;
        if (nbLineDisplayed > MAX_LCD_LINES) {
            M5.Display.clear(TFT_BLACK);
            M5.Display.setCursor(0, 0);
            nbLineDisplayed = 1; // Remet à 1 car on va afficher une ligne
        }
        
        M5.Display.println(message);
    }
}

/**
 * @brief Enregistre une ligne vide sur les sorties configurées
 * 
 * Cette méthode utilise la fonction log() avec un caractère de nouvelle ligne
 * pour créer une ligne vide dans les logs.
 */
void Logger::log() {
    log("");
}

/**
 * @brief Enregistre un message formaté (style printf)
 * 
 * @param format Chaîne de format au style printf
 * @param ... Arguments variables
 */
void Logger::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(String(buffer));
}

/**
 * @brief Enregistre un message sur les sorties configurées sans retour à la ligne
 * 
 * @param message Message sous forme de chaîne de caractères à enregistrer
 */
void Logger::print(const String& message) {
    // Sortie sur le port série si activée
    if (enableSerialOutput) {
        Serial.print(message);
        Serial.flush();
    }
    
    // Sortie sur l'écran LCD si activée
    if (enableLcdOutput) {
        M5.Display.print(message);
    }
}

/**
 * @brief Enregistre une ligne vide sur les sorties configurées sans retour à la ligne
 * 
 * Cette méthode utilise la fonction print() avec une chaîne vide
 * pour créer un saut de ligne dans les logs.
 */
void Logger::print() {
    print("");
}

/**
 * @brief Enregistre un message formaté sans retour à la ligne (style printf)
 * 
 * @param format Chaîne de format au style printf
 * @param ... Arguments variables
 */
void Logger::printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    print(String(buffer));
}

/**
 * @brief Active ou désactive la sortie série
 * 
 * @param enabled true pour activer, false pour désactiver
 */
void Logger::setSerialOutput(bool enabled) {
    enableSerialOutput = enabled;
}

/**
 * @brief Active ou désactive la sortie LCD
 * 
 * @param enabled true pour activer, false pour désactiver
 */
void Logger::setLcdOutput(bool enabled) {
    enableLcdOutput = enabled;
    if (enableLcdOutput) {
        clearLcd();
    }
}

bool Logger::getSerialOutput() {
    return enableSerialOutput;
}

bool Logger::getLcdOutput() {
    return enableLcdOutput;
}

/**
 * @brief Définit la taille du texte pour l'affichage LCD
 * 
 * @param size Taille du texte (1 = petit, 2 = moyen, 3 = grand)
 */
void Logger::setTextSize(int size) {
    textSize = size;
    if (enableLcdOutput) {
        M5.Display.setTextSize(textSize);
    }
}

/**
 * @brief Efface l'écran LCD et remet le compteur de lignes à zéro
 */
void Logger::clearLcd() {
    if (enableLcdOutput) {
        M5.Display.clear(TFT_BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextSize(textSize);
        nbLineDisplayed = 0;
    }
}

/**
 * @brief Obtient le nombre de lignes actuellement affichées sur l'écran LCD
 * 
 * @return int Nombre de lignes affichées
 */
int Logger::getLcdLineCount() {
    return nbLineDisplayed;
}
