/**
 * @file Logger.h
 * @brief Gestionnaire centralisé de logs pour le joystick de contrôle
 * @author Philippe Hubert
 * @date 2025
 *
 * Cette classe fournit un système de logging centralisé permettant d'afficher
 * les messages à la fois sur le moniteur série et sur l'écran LCD M5Stack Atom S3.
 *
 * @copyright Copyright (c) 2025
 * @version 1.0
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <M5Unified.h>

/**
 * @brief Classe utilitaire pour la gestion centralisée des logs et affichages
 *
 * La classe Logger fournit un système de logging unifié permettant d'afficher
 * des messages simultanément sur le port série et sur un écran LCD. Elle gère
 * automatiquement l'effacement de l'écran LCD après un nombre défini de lignes
 * pour éviter le débordement d'affichage.
 *
 * Caractéristiques principales :
 * - Sortie configurable sur port série (USBSerial) et/ou écran LCD
 * - Gestion automatique de l'effacement de l'écran LCD
 * - Compteur de lignes affiché sur LCD
 * - Interface statique ne nécessitant pas d'instanciation
 * - Adaptation pour M5Stack Atom S3 (écran 128x128 pixels)
 *
 * @note Toutes les méthodes sont statiques, aucune instanciation n'est nécessaire
 * @note L'écran LCD est automatiquement effacé après un nombre défini de lignes
 * @note Utilise USBSerial au lieu de Serial pour le M5Stack Atom S3
 */
class Logger
{
private:
    static int nbLineDisplayed;          ///< Compteur de lignes affichées sur l'écran LCD
    static bool enableSerialOutput;      ///< Flag pour activer/désactiver la sortie série
    static bool enableLcdOutput;         ///< Flag pour activer/désactiver la sortie LCD
    static const int MAX_LCD_LINES = 10; ///< Nombre maximum de lignes avant effacement (écran 128x128)
    static int textSize;                 ///< Taille du texte pour l'affichage LCD

public:
    /**
     * @brief Initialise le système de logging
     *
     * @param serialEnabled Active ou désactive la sortie sur le port série
     * @param lcdEnabled Active ou désactive la sortie sur l'écran LCD
     */
    static void init(bool serialEnabled = true, bool lcdEnabled = false);

    /**
     * @brief Enregistre un message sur les sorties configurées
     *
     * Cette méthode gère l'enregistrement des informations en :
     * - Imprimant sur le Moniteur Série (USBSerial) si activé
     * - Gérant l'affichage LCD avec effacement automatique si activé
     *
     * @param message Message sous forme de chaîne de caractères à enregistrer
     */
    static void log(const String &message);

    /**
     * @brief Enregistre une ligne vide sur les sorties configurées
     *
     * Cette méthode utilise la fonction log() avec un caractère de nouvelle ligne
     * pour créer une ligne vide dans les logs.
     */
    static void log(); // Surcharge pour ligne vide

    /**
     * @brief Enregistre un message formaté (style printf)
     *
     * @param format Chaîne de format au style printf
     * @param ... Arguments variables
     */
    static void logf(const char* format, ...);

    /**
     * @brief Enregistre un message sur les sorties configurées sans retour à la ligne
     *
     * @param message Message sous forme de chaîne de caractères à enregistrer
     */
    static void print(const String &message);

    /**
     * @brief Enregistre une ligne vide sur les sorties configurées
     *
     * Cette méthode utilise la fonction log() avec un caractère de nouvelle ligne
     * pour créer une ligne vide dans les logs.
     */
    static void print();

    /**
     * @brief Enregistre un message formaté sans retour à la ligne (style printf)
     *
     * @param format Chaîne de format au style printf
     * @param ... Arguments variables
     */
    static void printf(const char* format, ...);

    /**
     * @brief Active ou désactive la sortie série
     *
     * @param enabled true pour activer, false pour désactiver
     */
    static void setSerialOutput(bool enabled);

    /**
     * @brief Active ou désactive la sortie LCD
     *
     * @param enabled true pour activer, false pour désactiver
     */
    static void setLcdOutput(bool enabled);

    /**
     * @brief Définit la taille du texte pour l'affichage LCD
     *
     * @param size Taille du texte (1 = petit, 2 = moyen, 3 = grand)
     */
    static void setTextSize(int size);

    /**
     * @brief Efface l'écran LCD et remet le compteur de lignes à zéro
     */
    static void clearLcd();

    /**
     * @brief Obtient le nombre de lignes actuellement affichées sur l'écran LCD
     *
     * @return int Nombre de lignes affichées
     */
    static int getLcdLineCount();
};

#endif // LOGGER_H
