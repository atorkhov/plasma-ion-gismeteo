/***************************************************************************
 *   Copyright (C) 2012 by Alexey Torkhov <atorkhov@gmail.com>             *
 *                                                                         *
 *   Based on Environment Canada Ion by Shawn Starr                        *
 *   Copyright (C) 2007-2009 by Shawn Starr <shawn.starr@rogers.com>       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

/* Ion for Gismeteo data */

#ifndef ION_GISMETEO_H
#define ION_GISMETEO_H

#include <QtXml/QXmlStreamReader>
#include <QDateTime>

#include <kdemacros.h>
#include <KIO/Job>
#include <Plasma/DataEngine>
#include <Plasma/Weather/Ion>

class WeatherData
{

public:
    // WeatherEvent can have more than one, especially in Canada, eh? :)
    struct WeatherEvent {
        QString url;
        QString type;
        QString priority;
        QString description;
        QString timestamp;
    };

    // Five day forecast
    struct ForecastInfo {
        QString forecastPeriod;
        QString forecastSummary;
        QString iconName;
        QString shortForecast;

        QString forecastTempHigh;
        QString forecastTempLow;
        QString popPrecent;
        QString windForecast;

        QString precipForecast;
        QString precipType;
        QString precipTotalExpected;
        int forecastHumidity;
    };

    QString countryName;
    QString longTerritoryName;
    QString shortTerritoryName;
    QString cityName;
    QString regionName;
    QString stationID;
    QString stationLat;
    QString stationLon;

    // Current observation information.
    QString obsTimestamp;

    // Icon info to aproximate periods
    int iconPeriodHour;
    int iconPeriodMinute;

    QString condition;
    QString temperature;
    QString dewpoint;

    // In winter windchill, in summer, humidex
    QString comforttemp;

    float pressure;
    QString pressureTendency;

    float visibility;
    QString humidity;

    QString windSpeed;
    QString windGust;
    QString windDirection;
    QString windDegrees;

    QVector <WeatherData::WeatherEvent *> watches;
    QVector <WeatherData::WeatherEvent *> warnings;

    QString normalHigh;
    QString normalLow;

    QString forecastTimestamp;

    QString UVIndex;
    QString UVRating;

    // 5 day Forecast
    QVector <WeatherData::ForecastInfo *> forecasts;

    // Historical data from previous day.
    QString prevHigh;
    QString prevLow;
    QString prevPrecipType;
    QString prevPrecipTotal;

    // Almanac info
    QString sunriseTimestamp;
    QString sunsetTimestamp;
    QString moonriseTimestamp;
    QString moonsetTimestamp;

    // Historical Records
    float recordHigh;
    float recordLow;
    float recordRain;
    float recordSnow;
};

class KDE_EXPORT EnvGismeteoIon : public IonInterface
{
    Q_OBJECT

public:
    EnvGismeteoIon(QObject *parent, const QVariantList &args);
    ~EnvGismeteoIon();
    bool updateIonSource(const QString& source); // Sync data source with Applet
    void updateWeather(const QString& source);

public Q_SLOTS:
    virtual void reset();

protected:
    void init();  // Setup the city location, fetching the correct URL name.

protected Q_SLOTS:
    void slotDataArrived(KIO::Job *, const QByteArray &);
    void slotJobFinished(KJob *);

private:
    /* Environment Canada Methods - Internal for Ion */
    void deleteForecasts();

    QMap<QString, ConditionIcons> setupConditionIconMappings(void) const;
    QMap<QString, ConditionIcons> setupForecastIconMappings(void) const;

    QMap<QString, ConditionIcons> const& conditionIcons(void) const;
    QMap<QString, ConditionIcons> const& forecastIcons(void) const;

    // Load and parse the specific place(s)
    void getXMLData(const QString& source);
    bool readXMLData(const QString& source, const QByteArray& xml);

    // Check if place specified is valid or not
    QStringList validate(const QString& source) const;

    // Weather information
    QHash<QString, WeatherData> m_weatherData;

    // Store KIO jobs
    QHash<KJob *, QByteArray> m_jobXml;
    QHash<KJob *, QString> m_jobList;

};

K_EXPORT_PLASMA_DATAENGINE(gismeteo, EnvGismeteoIon)

#endif
