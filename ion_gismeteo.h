/***************************************************************************
 *   Copyright (C) 2012 by Alexey Torkhov <atorkhov@gmail.com>             *
 *                                                                         *
 *   Based on KDE weather ions by Shawn Starr                              *
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

    // Current observation information.
    QString date;
    QString condition;
    QString conditionIcon;
    QString temperature;
    QString pressure;
    QString windDirection;
    QString windSpeed;
    QString humidity;
    QString waterTemperature;

    struct Forecast
    {
        QString day;
        QString icon;
        QString temperatureHigh;
        QString temperatureLow;
    };
    QList<Forecast> forecasts;

};

class KDE_EXPORT EnvGismeteoIon : public IonInterface
{
    Q_OBJECT

public:
    EnvGismeteoIon(QObject *parent, const QVariantList &args);
    ~EnvGismeteoIon();
    bool updateIonSource(const QString& source); // Sync data source with Applet
    void updateWeather(const QString& source);
    void validate(const QString& source);

    struct XMLMapInfo {
        QString name;
        QString link;
        int id;
    };

public Q_SLOTS:
    virtual void reset();

protected:
    void init();  // Setup the city location, fetching the correct URL name.

protected Q_SLOTS:
    void slotDataArrived(KIO::Job *, const QByteArray &);
    void slotJobFinished(KJob *);

    void setup_slotDataArrived(KIO::Job *, const QByteArray &);
    void setup_slotJobFinished(KJob *);

private:
    /* Gismeteo Methods - Internal for Ion */
    void deleteForecasts();

    QMap<QString, ConditionIcons> setupConditionIconMappings(void) const;
    QMap<QString, ConditionIcons> setupForecastIconMappings(void) const;
    QMap<QString, QString> setupForecastConditionMappings(void) const;
    QMap<QString, QString> setupDayMappings(void) const;
    QMap<QString, WindDirections> setupWindIconMappings(void) const;

    QMap<QString, ConditionIcons> const& conditionIcons(void) const;
    QMap<QString, ConditionIcons> const& forecastIcons(void) const;
    QMap<QString, QString> const& forecastConditions(void) const;
    QMap<QString, QString> const& dayMap(void) const;
    QMap<QString, WindDirections> const& windIcons(void) const;

    // Load and parse the specific place(s)
    void getWeather(const QString& code, const QString& source);
    bool readHTMLData(const QString& source, const QByteArray& xml);

    // Check if place specified is valid or not
    void findPlace(const QString& place, const QString& source);
    bool readSearchHTMLData(const QString& source, const QByteArray& xml);

    // Weather information
    QHash<QString, WeatherData> m_weatherData;
    QHash<QString, QList<EnvGismeteoIon::XMLMapInfo> > m_places;

    // Store KIO jobs
    QHash<KJob *, QByteArray> m_jobXml;
    QHash<KJob *, QString> m_jobList;

    QHash<KJob *, QByteArray> m_searchJobXml;
    QHash<KJob *, QString> m_searchJobList;


};

K_EXPORT_PLASMA_DATAENGINE(gismeteo, EnvGismeteoIon)

#endif
