/***************************************************************************
 *   Copyright (C) 2012 by Alexey Torkhov <atorkhov@gmail.com>             *
 *                                                                         *
 *   Based on KDE weather ions by Shawn Starr                              *
 *   Copyright (C) 2007-2011 by Shawn Starr <shawn.starr@rogers.com>       *
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

#include "ion_gismeteo.h"

#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlFormatter>
#include <QFile>
#include <QBuffer>
#include <QStack>

#include <KIO/Job>
#include <KStandardDirs>
#include <KUnitConversion/Converter>
#include <Solid/Networking>
#include <Plasma/DataContainer>

#include <qlibxmlnodemodel.h>

// Receiver for html weather data
class Receiver : public QAbstractXmlReceiver
{
public:
    Receiver(const QXmlNamePool &namePool, WeatherData &weatherData);
    void atomicValue(const QVariant &);
    void endElement();
    void startElement(const QXmlName &name);

    void attribute(const QXmlName &, const QStringRef &) {};
    void characters(const QStringRef &) {}
    void comment(const QString &) {}
    void endDocument() {}
    void endOfSequence() {}
    void namespaceBinding(const QXmlName &) {}
    void processingInstruction(const QXmlName &, const QString &) {}
    void startDocument() {}
    void startOfSequence() {}

    WeatherData &m_weatherData;

private:
    QXmlNamePool m_namePool;
    QStack<QString> m_elements;
};

Receiver::Receiver(const QXmlNamePool &namePool, WeatherData &weatherData)
    : m_weatherData(weatherData), m_namePool(namePool)
{
}

// Called for every element
void Receiver::startElement(const QXmlName &xmlname)
{
    QString name = xmlname.localName(m_namePool);
    m_elements.push(name);

    if (name == "forecast") {
        m_weatherData.forecasts.append(WeatherData::Forecast());
    }
}

void Receiver::endElement()
{
    m_elements.pop();
}

// Called for every text node
void Receiver::atomicValue(const QVariant &val)
{
    QString value = val.toString();
    QString currentElement = m_elements.top();

    kDebug() << currentElement << value;

    if (m_weatherData.forecasts.empty()) {
        if (currentElement == "date") {
            m_weatherData.date = value;
        } else if (currentElement == "condition") {
            m_weatherData.condition = value;
        } else if (currentElement == "temperature") {
            if (value.endsWith(QString::fromUtf8("°C"))) {
                value.chop(2);
            }
            m_weatherData.temperature = value;
        } else if (currentElement == "pressure") {
            if (value.endsWith(QString::fromUtf8("мм рт.ст."))) {
                value.chop(9);
            }
            m_weatherData.pressure = value;
        } else if (currentElement == "windDirection") {
            m_weatherData.windDirection = value;
        } else if (currentElement == "windSpeed") {
            if (value.endsWith(QString::fromUtf8("м/с"))) {
                value.chop(3);
            }
            m_weatherData.windSpeed = value;
        } else if (currentElement == "humidity") {
            if (value.endsWith(QString::fromUtf8("%"))) {
                value.chop(1);
            }
            m_weatherData.humidity = value;
        } else if (currentElement == "waterTemperature") {
            if (value.endsWith(QString::fromUtf8("°C"))) {
                value.chop(2);
            }
            m_weatherData.waterTemperature = value;
        }
    } else {
        WeatherData::Forecast &currentForecast = m_weatherData.forecasts.back();

        if (currentElement == "day") {
            currentForecast.day = value;
        } else if (currentElement == "icon") {
            currentForecast.icon = value;
        } else if (currentElement == "temperature") {
            QStringList temp = value.split("..");
            if (temp.size() == 2) {
                currentForecast.temperatureHigh = temp.at(0);
                if (temp[0].endsWith(QString::fromUtf8("°"))) {
                    currentForecast.temperatureHigh.chop(1);
                }
                currentForecast.temperatureLow = temp.at(1);
                if (temp[1].endsWith(QString::fromUtf8("°"))) {
                    currentForecast.temperatureLow.chop(1);
                }
            }
        }
    }
}

// Receiver for html search data
class SearchReceiver : public QAbstractXmlReceiver
{
public:
    SearchReceiver(const QXmlNamePool &namePool, QList<EnvGismeteoIon::XMLMapInfo> &places);
    void atomicValue(const QVariant &);
    void endElement();
    void startElement(const QXmlName &name);

    void attribute(const QXmlName &, const QStringRef &) {};
    void characters(const QStringRef &) {}
    void comment(const QString &) {}
    void endDocument() {}
    void endOfSequence() {}
    void namespaceBinding(const QXmlName &) {}
    void processingInstruction(const QXmlName &, const QString &) {}
    void startDocument() {}
    void startOfSequence() {}

    QList<EnvGismeteoIon::XMLMapInfo> &m_places;

private:
    QXmlNamePool m_namePool;
    QStack<QString> m_elements;

    EnvGismeteoIon::XMLMapInfo m_currentPlace;
};

SearchReceiver::SearchReceiver(const QXmlNamePool &namePool, QList<EnvGismeteoIon::XMLMapInfo> &places)
    : m_places(places), m_namePool(namePool)
{
}

// Called for every element
void SearchReceiver::startElement(const QXmlName &xmlname)
{
    QString name = xmlname.localName(m_namePool);
    m_elements.push(name);

    if (name == "place") {
        m_currentPlace = EnvGismeteoIon::XMLMapInfo();
        m_currentPlace.id = 0;
    }
}

void SearchReceiver::endElement()
{
    QString currentElement = m_elements.top();

    if (currentElement == "place") {
        m_places.append(m_currentPlace);
    }

    m_elements.pop();
}

// Called for every text nodeurl
void SearchReceiver::atomicValue(const QVariant &val)
{
    QString value = val.toString();
    QString currentElement = m_elements.top();

    kDebug() << currentElement << value;

    if (currentElement == "name") {
        m_currentPlace.name = value;
    } else if (currentElement == "link") {
        m_currentPlace.link = value;

        QRegExp rxlink("/city/daily/([0-9]+)/");
        int pos = rxlink.indexIn(value);
        if (pos > -1) {
            m_currentPlace.id = rxlink.cap(1).toInt();
        }
    }
}


// ctor, dtor
EnvGismeteoIon::EnvGismeteoIon(QObject *parent, const QVariantList &args)
        : IonInterface(parent, args)
{
}

void EnvGismeteoIon::reset()
{
}

EnvGismeteoIon::~EnvGismeteoIon()
{
}

// Get the master list of locations to be parsed
void EnvGismeteoIon::init()
{
    kDebug() << "init()";

    setInitialized(true);
}

QMap<QString, IonInterface::ConditionIcons> EnvGismeteoIon::setupConditionIconMappings(void) const
{
    QMap<QString, ConditionIcons> conditionList;

    return conditionList;
}

QMap<QString, IonInterface::ConditionIcons> EnvGismeteoIon::setupForecastIconMappings(void) const
{
    QMap<QString, ConditionIcons> forecastList;

    // Clouds
    forecastList[QString::fromUtf8("d.sun.png")]            = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.png")]         = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c2.png")]         = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c3.png")]         = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c4.png")]         = Overcast;

    // Clouds + rain
    forecastList[QString::fromUtf8("d.sun.r1.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.r2.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.r3.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.r4.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r2.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r3.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r4.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.r2.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.r3.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c2.r4.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c3.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c3.r2.png")]      = LightRain;
    forecastList[QString::fromUtf8("d.sun.c3.r3.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c3.r4.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c4.r1.png")]      = Showers;
    forecastList[QString::fromUtf8("d.sun.c4.r2.png")]      = LightRain;
    forecastList[QString::fromUtf8("d.sun.c4.r3.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c4.r4.png")]      = Rain;

    // Clouds + thunderstorm
    forecastList[QString::fromUtf8("d.sun.st.png")]         = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = Overcast;

    // Clouds + rain + thunderstorm
    forecastList[QString::fromUtf8("d.sun.r1.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.r2.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.r3.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.r4.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r3.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r4.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.r2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.r3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c2.r4.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c3.r2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.r3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.r4.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r1.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r4.st.png")]   = Thunderstorm;

    // Clouds + snow
    forecastList[QString::fromUtf8("d.sun.s1.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.s2.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.s3.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.s4.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s2.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s3.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s4.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c2.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c2.s2.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c2.s3.png")]      = Snow;
    forecastList[QString::fromUtf8("d.sun.c2.s4.png")]      = Snow;
    forecastList[QString::fromUtf8("d.sun.c3.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c3.s2.png")]      = LightSnow;
    forecastList[QString::fromUtf8("d.sun.c3.s3.png")]      = Snow;
    forecastList[QString::fromUtf8("d.sun.c3.s4.png")]      = Snow;
    forecastList[QString::fromUtf8("d.sun.c4.s1.png")]      = Flurries;
    forecastList[QString::fromUtf8("d.sun.c4.s2.png")]      = LightSnow;
    forecastList[QString::fromUtf8("d.sun.c4.s3.png")]      = Snow;
    forecastList[QString::fromUtf8("d.sun.c4.s4.png")]      = Snow;

    // Clouds + snow + thunderstorm
    forecastList[QString::fromUtf8("d.sun.s1.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.s2.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.s3.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.s4.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s3.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s4.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.s2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.s3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c2.s4.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c3.s2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.s3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.s4.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s1.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s4.st.png")]   = Thunderstorm;
 
    return forecastList;
}

QMap<QString, QString> EnvGismeteoIon::setupForecastConditionMappings(void) const
{
    QMap<QString, QString> forecastList;

    // Clouds
    forecastList[QString::fromUtf8("d.sun.png")]            = QString::fromUtf8("Ясно");
    forecastList[QString::fromUtf8("d.sun.c1.png")]         = QString::fromUtf8("Малооблачно");
    forecastList[QString::fromUtf8("d.sun.c2.png")]         = QString::fromUtf8("Малооблачно");
    forecastList[QString::fromUtf8("d.sun.c3.png")]         = QString::fromUtf8("Облачно");
    forecastList[QString::fromUtf8("d.sun.c4.png")]         = QString::fromUtf8("Пасмурно");

    // Clouds + rain
    forecastList[QString::fromUtf8("d.sun.r1.png")]         = QString::fromUtf8("Ясно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.r2.png")]         = QString::fromUtf8("Ясно, дождь");
    forecastList[QString::fromUtf8("d.sun.r3.png")]         = QString::fromUtf8("Ясно, сильный дождь");
    forecastList[QString::fromUtf8("d.sun.r4.png")]         = QString::fromUtf8("Ясно, ливень");
    forecastList[QString::fromUtf8("d.sun.c1.r1.png")]      = QString::fromUtf8("Малооблачно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c1.r2.png")]      = QString::fromUtf8("Малооблачно, дождь");
    forecastList[QString::fromUtf8("d.sun.c1.r3.png")]      = QString::fromUtf8("Малооблачно, сильный дождь");
    forecastList[QString::fromUtf8("d.sun.c1.r4.png")]      = QString::fromUtf8("Малооблачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c2.r1.png")]      = QString::fromUtf8("Малооблачно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c2.r2.png")]      = QString::fromUtf8("Малооблачно, дождь");
    forecastList[QString::fromUtf8("d.sun.c2.r3.png")]      = QString::fromUtf8("Малооблачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c2.r4.png")]      = QString::fromUtf8("Малооблачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c3.r1.png")]      = QString::fromUtf8("Облачно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c3.r2.png")]      = QString::fromUtf8("Облачно, дождь");
    forecastList[QString::fromUtf8("d.sun.c3.r3.png")]      = QString::fromUtf8("Облачно, сильный дождь");
    forecastList[QString::fromUtf8("d.sun.c3.r4.png")]      = QString::fromUtf8("Облачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c4.r1.png")]      = QString::fromUtf8("Пасмурно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c4.r2.png")]      = QString::fromUtf8("Пасмурно, дождь");
    forecastList[QString::fromUtf8("d.sun.c4.r3.png")]      = QString::fromUtf8("Пасмурно, сильный дождь");
    forecastList[QString::fromUtf8("d.sun.c4.r4.png")]      = QString::fromUtf8("Пасмурно, ливень");

    // Clouds + thunderstorm
    forecastList[QString::fromUtf8("d.sun.st.png")]         = QString::fromUtf8("Ясно, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = QString::fromUtf8("Малооблачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = QString::fromUtf8("Малооблачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = QString::fromUtf8("Облачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = QString::fromUtf8("Пасмурно, гроза");

    // Clouds + rain + thunderstorm
    forecastList[QString::fromUtf8("d.sun.c1.r1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r2.st.png")]   = QString::fromUtf8("Малооблачно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r3.st.png")]   = QString::fromUtf8("Малооблачно, сильный дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r4.st.png")]   = QString::fromUtf8("Малооблачно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r2.st.png")]   = QString::fromUtf8("Малооблачно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r3.st.png")]   = QString::fromUtf8("Малооблачно, сильный дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r4.st.png")]   = QString::fromUtf8("Малооблачно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r1.st.png")]   = QString::fromUtf8("Облачно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r2.st.png")]   = QString::fromUtf8("Облачно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r3.st.png")]   = QString::fromUtf8("Облачно, сильный дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r4.st.png")]   = QString::fromUtf8("Облачно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r1.st.png")]   = QString::fromUtf8("Пасмурно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r2.st.png")]   = QString::fromUtf8("Пасмурно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r3.st.png")]   = QString::fromUtf8("Пасмурно, сильный дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r4.st.png")]   = QString::fromUtf8("Облачно, ливень, гроза");

    // Clouds + snow
    forecastList[QString::fromUtf8("d.sun.s1.png")]         = QString::fromUtf8("Ясно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.s2.png")]         = QString::fromUtf8("Ясно, снег");
    forecastList[QString::fromUtf8("d.sun.s3.png")]         = QString::fromUtf8("Ясно, сильный снег");
    forecastList[QString::fromUtf8("d.sun.s4.png")]         = QString::fromUtf8("Ясно, буран");
    forecastList[QString::fromUtf8("d.sun.c1.s1.png")]      = QString::fromUtf8("Малооблачно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c1.s2.png")]      = QString::fromUtf8("Малооблачно, снег");
    forecastList[QString::fromUtf8("d.sun.c1.s3.png")]      = QString::fromUtf8("Малооблачно, сильный снег");
    forecastList[QString::fromUtf8("d.sun.c1.s4.png")]      = QString::fromUtf8("Малооблачно, буран");
    forecastList[QString::fromUtf8("d.sun.c2.s1.png")]      = QString::fromUtf8("Малооблачно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c2.s2.png")]      = QString::fromUtf8("Малооблачно, снег");
    forecastList[QString::fromUtf8("d.sun.c2.s3.png")]      = QString::fromUtf8("Малооблачно, сильный снег");
    forecastList[QString::fromUtf8("d.sun.c2.s4.png")]      = QString::fromUtf8("Малооблачно, буран");
    forecastList[QString::fromUtf8("d.sun.c3.s1.png")]      = QString::fromUtf8("Облачно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c3.s2.png")]      = QString::fromUtf8("Облачно, снег");
    forecastList[QString::fromUtf8("d.sun.c3.s3.png")]      = QString::fromUtf8("Облачно, сильный снег");
    forecastList[QString::fromUtf8("d.sun.c3.s4.png")]      = QString::fromUtf8("Облачно, буран");
    forecastList[QString::fromUtf8("d.sun.c4.s1.png")]      = QString::fromUtf8("Пасмурно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c4.s2.png")]      = QString::fromUtf8("Пасмурно, снег");
    forecastList[QString::fromUtf8("d.sun.c4.s3.png")]      = QString::fromUtf8("Пасмурно, сильный снег");
    forecastList[QString::fromUtf8("d.sun.c4.s4.png")]      = QString::fromUtf8("Пасмурно, буран");

    // Clouds + snow + thunderstorm
    forecastList[QString::fromUtf8("d.sun.s1.st.png")]      = QString::fromUtf8("Ясно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.s2.st.png")]      = QString::fromUtf8("Ясно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.s3.st.png")]      = QString::fromUtf8("Ясно, сильный снег, гроза");
    forecastList[QString::fromUtf8("d.sun.s4.st.png")]      = QString::fromUtf8("Ясно, буран, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s2.st.png")]   = QString::fromUtf8("Малооблачно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s3.st.png")]   = QString::fromUtf8("Малооблачно, сильный снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s4.st.png")]   = QString::fromUtf8("Малооблачно, буран, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s2.st.png")]   = QString::fromUtf8("Малооблачно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s3.st.png")]   = QString::fromUtf8("Малооблачно, сильный снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s4.st.png")]   = QString::fromUtf8("Малооблачно, буран, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s1.st.png")]   = QString::fromUtf8("Облачно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s2.st.png")]   = QString::fromUtf8("Облачно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s3.st.png")]   = QString::fromUtf8("Облачно, сильный снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s4.st.png")]   = QString::fromUtf8("Облачно, буран, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s1.st.png")]   = QString::fromUtf8("Пасмурно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s2.st.png")]   = QString::fromUtf8("Пасмурно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s3.st.png")]   = QString::fromUtf8("Пасмурно, сильный снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s4.st.png")]   = QString::fromUtf8("Пасмурно, буран, гроза");

    return forecastList;
}

QMap<QString, QString> EnvGismeteoIon::setupDayMappings(void) const
{
    QMap<QString, QString> days;
    days[QString::fromUtf8("пн")] = QDate::shortDayName(1, QDate::StandaloneFormat);
    days[QString::fromUtf8("вт")] = QDate::shortDayName(2, QDate::StandaloneFormat);
    days[QString::fromUtf8("ср")] = QDate::shortDayName(3, QDate::StandaloneFormat);
    days[QString::fromUtf8("чт")] = QDate::shortDayName(4, QDate::StandaloneFormat);
    days[QString::fromUtf8("пт")] = QDate::shortDayName(5, QDate::StandaloneFormat);
    days[QString::fromUtf8("сб")] = QDate::shortDayName(6, QDate::StandaloneFormat);
    days[QString::fromUtf8("вс")] = QDate::shortDayName(7, QDate::StandaloneFormat);
    return days;
}

QMap<QString, IonInterface::WindDirections> EnvGismeteoIon::setupWindIconMappings(void) const
{
    QMap<QString, WindDirections> windDir;
    windDir[QString::fromUtf8("с")] = N;
    windDir[QString::fromUtf8("св")] = NE;
    windDir[QString::fromUtf8("ю")] = S;
    windDir[QString::fromUtf8("юз")] = SW;
    windDir[QString::fromUtf8("в")] = E;
    windDir[QString::fromUtf8("юв")] = SE;
    windDir[QString::fromUtf8("з")] = W;
    windDir[QString::fromUtf8("сз")] = NW;
    windDir[QString::fromUtf8("")] = VR;
    return windDir;
}

QMap<QString, IonInterface::ConditionIcons> const& EnvGismeteoIon::conditionIcons(void) const
{
    static QMap<QString, ConditionIcons> const condval = setupConditionIconMappings();
    return condval;
}

QMap<QString, IonInterface::ConditionIcons> const& EnvGismeteoIon::forecastIcons(void) const
{
    static QMap<QString, ConditionIcons> const foreval = setupForecastIconMappings();
    return foreval;
}

QMap<QString, QString> const& EnvGismeteoIon::forecastConditions(void) const
{
    static QMap<QString, QString> const foreval = setupForecastConditionMappings();
    return foreval;
}

QMap<QString, QString> const& EnvGismeteoIon::dayMap(void) const
{
    static QMap<QString, QString> const dayval = setupDayMappings();
    return dayval;
}

QMap<QString, IonInterface::WindDirections> const& EnvGismeteoIon::windIcons(void) const
{
    static QMap<QString, WindDirections> const wval = setupWindIconMappings();
    return wval;
}

// Get a specific Ion's data
bool EnvGismeteoIon::updateIonSource(const QString& source)
{
    kDebug() << "updateIonSource()" << source;

    // We expect the applet to send the source in the following tokenization:
    // ionname|validate|place_name - Triggers validation of place
    // ionname|weather|place_name - Triggers receiving weather of place

    QStringList sourceAction = source.split('|');

    // Guard: if the size of array is not 2 then we have bad data, return an error
    if (sourceAction.size() < 2) {
        setData(source, "validate", "gismeteo|malformed");
        return true;
    }

    if (sourceAction[1] == "validate" && sourceAction.size() > 2) {
        findPlace(sourceAction[2], source);
        return true;
    } else if (sourceAction[1] == "weather" && sourceAction.size() > 3) {
        getWeather(sourceAction[3], source);
        return true;
    } else {
        setData(source, "validate", "gismeteo|malformed");
        return true;
    }
    return false;
}

// Gets weather for a city
void EnvGismeteoIon::getWeather(const QString& code, const QString& source)
{
    foreach (const QString &fetching, m_jobList) {
        if (fetching == source) {
            // already getting this source and awaiting the data
            return;
        }
    }

    kDebug() << source;

    KUrl url = QString("http://www.gismeteo.ru/city/daily/" + code + "/");
    //url = "file:///home/alex/Develop/kde/plasma-ion-gismeteo/4368.html";
    kDebug() << "Will Try URL: " << url;

    KIO::TransferJob* const newJob  = KIO::get(url.url(), KIO::Reload, KIO::HideProgressInfo);

    m_jobXml.insert(newJob, QByteArray());
    m_jobList.insert(newJob, source);

    connect(newJob, SIGNAL(data(KIO::Job*,QByteArray)), this,
            SLOT(slotDataArrived(KIO::Job*,QByteArray)));
    connect(newJob, SIGNAL(result(KJob*)), this, SLOT(slotJobFinished(KJob*)));
}

// Search for a city
void EnvGismeteoIon::findPlace(const QString& place, const QString& source)
{
    KUrl url = QString("http://www.gismeteo.ru/city/?gis0=" + place + "&searchQueryData=");
    //url = "file:///home/alex/Develop/kde/plasma-ion-gismeteo/search.html";
    kDebug() << "Will Try URL: " << url;

    KIO::TransferJob* const newJob  = KIO::get(url.url(), KIO::Reload, KIO::HideProgressInfo);

    m_searchJobXml.insert(newJob, QByteArray());
    m_searchJobList.insert(newJob, source);

    connect(newJob, SIGNAL(data(KIO::Job*,QByteArray)), this,
            SLOT(setup_slotDataArrived(KIO::Job*,QByteArray)));
    connect(newJob, SIGNAL(result(KJob*)), this, SLOT(setup_slotJobFinished(KJob*)));
}

void EnvGismeteoIon::slotDataArrived(KIO::Job *job, const QByteArray &data)
{
    if (data.isEmpty() || !m_jobXml.contains(job)) {
        return;
    }

    m_jobXml[job].append(data);
}

void EnvGismeteoIon::slotJobFinished(KJob *job)
{
    // Dual use method, if we're fetching location data to parse we need to do this first
    const QString source = m_jobList.value(job);
    setData(source, Data());

    const QByteArray &data = m_jobXml.value(job);
    readHTMLData(source, data);
    updateWeather(source);

    m_jobList.remove(job);
    m_jobXml.remove(job);
}

void EnvGismeteoIon::setup_slotDataArrived(KIO::Job *job, const QByteArray &data)
{
    if (data.isEmpty() || !m_searchJobXml.contains(job)) {
        return;
    }

    m_searchJobXml[job].append(data);
}

void EnvGismeteoIon::setup_slotJobFinished(KJob *job)
{
    // Dual use method, if we're fetching location data to parse we need to do this first
    const QString source = m_searchJobList.value(job);
    setData(source, Data());

    const QByteArray &data = m_searchJobXml.value(job);
    readSearchHTMLData(source, data);
    validate(source);

    m_searchJobList.remove(job);
    m_searchJobXml.remove(job);
}

// Parse Weather
bool EnvGismeteoIon::readHTMLData(const QString& source, const QByteArray& xml)
{
    WeatherData data;

    QXmlQuery query;

    kDebug() << "readHTMLData()";

    // Setup model
    QLibXmlNodeModel model(query.namePool(), xml, QUrl());
    query.setFocus(model.dom());

    // Setup query
    QFile queryFile;
    queryFile.setFileName(KGlobal::dirs()->findResource("data", "plasma-ion-gismeteo/gismeteo.xq"));
    if (!queryFile.open(QIODevice::ReadOnly)) {
        kDebug() << "Can't open XQuery file" << queryFile.fileName();
        return false;
    }
    query.setQuery(&queryFile, QUrl::fromLocalFile(queryFile.fileName()));

    if (!query.isValid()) {
        kDebug() << "query is not valid";
        return false;
    }

    // Setup a formatter
    Receiver receiver(query.namePool(), data);

    // Evaluate query
    query.evaluateTo(&receiver);

    m_weatherData[source] = data;

    return true;
}

// Parse search results
bool EnvGismeteoIon::readSearchHTMLData(const QString& source, const QByteArray& xml)
{
    QList<XMLMapInfo> data;

    QXmlQuery query;

    kDebug() << "readSearchHTMLData()" << source << xml;

    // Setup model
    QLibXmlNodeModel model(query.namePool(), xml, QUrl("file:///search"));
    query.setFocus(model.dom());

    // Setup query
    QFile queryFile;
    queryFile.setFileName(KGlobal::dirs()->findResource("data", "plasma-ion-gismeteo/gismeteo-search.xq"));
    if (!queryFile.open(QIODevice::ReadOnly)) {
        kDebug() << "Can't open XQuery file" << queryFile.fileName();
        return false;
    }
    query.setQuery(&queryFile, QUrl::fromLocalFile(queryFile.fileName()));

    if (!query.isValid()) {
        kDebug() << "query is not valid";
        return false;
    }

    // Setup a formatter
    SearchReceiver receiver(query.namePool(), data);

    // Evaluate query
    query.evaluateTo(&receiver);

    m_places[source] = data;

    return true;
}

void EnvGismeteoIon::updateWeather(const QString& source)
{
    Plasma::DataEngine::Data data;

    kDebug() << "updateWeather()";

    // Real weather - Current conditions
    data.insert("Current Conditions", i18nc("weather condition", m_weatherData[source].condition.toUtf8()));

    data.insert("Temperature", m_weatherData[source].temperature);
    data.insert("Temperature Unit", QString::number(KUnitConversion::Celsius));

    data.insert("Pressure", m_weatherData[source].pressure);
    data.insert("Pressure Unit", QString::number(KUnitConversion::MillimetersOfMercury));

    data.insert("Humidity", m_weatherData[source].humidity);
    data.insert("Humidity Unit", QString::number(KUnitConversion::Percent));

    data.insert("Wind Speed", m_weatherData[source].windSpeed);
    data.insert("Wind Speed Unit", QString::number(KUnitConversion::MeterPerSecond));
    data.insert("Wind Direction", getWindDirectionIcon(windIcons(), m_weatherData[source].windDirection));

    data.insert("Water Temperature", m_weatherData[source].waterTemperature);

    int dayIndex = 0;
    foreach(const WeatherData::Forecast &forecast, m_weatherData[source].forecasts) {
        data.insert(QString("Short Forecast Day %1").arg(dayIndex), QString("%1|%2|%3|%4|%5|%6")
                .arg(dayMap()[forecast.day.toLower()])
                .arg(getWeatherIcon(forecastIcons(), forecast.icon))
                .arg(forecastConditions().value(forecast.icon))
                .arg(forecast.temperatureHigh)
                .arg(forecast.temperatureLow)
                .arg("N/U"));
        dayIndex++;
    }

    // Set number of forecasts per day/night supported
    data.insert("Total Weather Days", dayIndex);

    data.insert("Credit", i18n("Meteorological data is provided by Gismeteo"));
    data.insert("Credit Url", "http://www.gismeteo.ru/");
    setData(source, data);

    kDebug() << data;
}

void EnvGismeteoIon::validate(const QString& source)
{
    kDebug() << "validate()";

    if (!m_places.contains(source)) {
        return;
    }

    const QList<XMLMapInfo> &data = m_places[source];

    QString placeList;
    bool beginflag = true;

    foreach(const XMLMapInfo &place, data) {
        if (beginflag) {
            placeList.append(QString("%1|extra|%2").arg(place.name).arg(place.id));
            beginflag = false;
        } else {
            placeList.append(QString("|place|%1|extra|%2").arg(place.name).arg(place.id));
        }
    }
    if (m_places.count() > 1) {
        setData(source, "validate", QString("gismeteo|valid|multiple|place|%1").arg(placeList));
    } else {
        setData(source, "validate", QString("gismeteo|valid|single|place|%1").arg(placeList));
    }
}

#include "ion_gismeteo.moc"
