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

#include "config.h"

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

void Receiver::atomicValue(const QVariant &val)
{
    QString value = val.toString();
    QString currentElement = m_elements.top();

    if (m_weatherData.forecasts.empty()) {
        kDebug() << currentElement << value;

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

        kDebug() << currentElement << value;

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

    conditionList[QString::fromUtf8("малооблачно, небольшой дождь")] = LightRain;
    conditionList[QString::fromUtf8("облачно")] = Overcast;
    conditionList[QString::fromUtf8("пасмурно")] = Overcast;
    conditionList[QString::fromUtf8("пасмурно, дымка")] = Mist;
    conditionList[QString::fromUtf8("пасмурно, небольшой дождь")] = LightRain;
    conditionList[QString::fromUtf8("пасмурно, дождь")] = Rain;

    return conditionList;
}

QMap<QString, IonInterface::ConditionIcons> EnvGismeteoIon::setupForecastIconMappings(void) const
{
    QMap<QString, ConditionIcons> forecastList;

    // Clouds + rain
    forecastList[QString::fromUtf8("d.sun.png")]            = ClearDay;
    forecastList[QString::fromUtf8("d.sun.r1.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.r2.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.r3.png")]         = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.png")]         = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r2.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c1.r3.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.png")]         = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c2.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.r2.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c2.r3.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c3.png")]         = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c3.r1.png")]      = ChanceShowersDay;
    forecastList[QString::fromUtf8("d.sun.c3.r2.png")]      = LightRain;
    forecastList[QString::fromUtf8("d.sun.c3.r3.png")]      = Rain;
    forecastList[QString::fromUtf8("d.sun.c4.png")]         = Overcast;
    forecastList[QString::fromUtf8("d.sun.c4.r1.png")]      = Showers;
    forecastList[QString::fromUtf8("d.sun.c4.r2.png")]      = LightRain;
    forecastList[QString::fromUtf8("d.sun.c4.r3.png")]      = Rain;

    // Clouds + rain + thunderstorm
    forecastList[QString::fromUtf8("d.sun.st.png")]         = ClearDay;
    forecastList[QString::fromUtf8("d.sun.r1.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.r2.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.r3.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.r3.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c2.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.r2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.r3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c3.r1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c3.r2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.r3.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = Overcast;
    forecastList[QString::fromUtf8("d.sun.c4.r1.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.r3.st.png")]   = Thunderstorm;

    // Clouds + snow
    //forecastList[QString::fromUtf8("d.sun.png")]            = ClearDay;
    forecastList[QString::fromUtf8("d.sun.s1.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.s2.png")]         = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.s3.png")]         = ChanceSnowDay;
    //forecastList[QString::fromUtf8("d.sun.c1.png")]         = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s2.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c1.s3.png")]      = ChanceSnowDay;
    //forecastList[QString::fromUtf8("d.sun.c2.png")]         = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c2.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c2.s2.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c2.s3.png")]      = Snow;
    //forecastList[QString::fromUtf8("d.sun.c3.png")]         = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c3.s1.png")]      = ChanceSnowDay;
    forecastList[QString::fromUtf8("d.sun.c3.s2.png")]      = LightSnow;
    forecastList[QString::fromUtf8("d.sun.c3.s3.png")]      = Snow;
    //forecastList[QString::fromUtf8("d.sun.c4.png")]         = Overcast;
    forecastList[QString::fromUtf8("d.sun.c4.s1.png")]      = Flurries;
    forecastList[QString::fromUtf8("d.sun.c4.s2.png")]      = LightSnow;
    forecastList[QString::fromUtf8("d.sun.c4.s3.png")]      = Snow;

    // Clouds + snow + thunderstorm
    //forecastList[QString::fromUtf8("d.sun.st.png")]         = ClearDay;
    forecastList[QString::fromUtf8("d.sun.s1.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.s2.st.png")]      = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.s3.st.png")]      = ChanceThunderstormDay;
    //forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = ClearDay;
    forecastList[QString::fromUtf8("d.sun.c1.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c1.s3.st.png")]   = ChanceThunderstormDay;
    //forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = FewCloudsDay;
    forecastList[QString::fromUtf8("d.sun.c2.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.s2.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c2.s3.st.png")]   = Thunderstorm;
    //forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = PartlyCloudyDay;
    forecastList[QString::fromUtf8("d.sun.c3.s1.st.png")]   = ChanceThunderstormDay;
    forecastList[QString::fromUtf8("d.sun.c3.s2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c3.s3.st.png")]   = Thunderstorm;
    //forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = Overcast;
    forecastList[QString::fromUtf8("d.sun.c4.s1.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s2.st.png")]   = Thunderstorm;
    forecastList[QString::fromUtf8("d.sun.c4.s3.st.png")]   = Thunderstorm;
 
    return forecastList;
}

QMap<QString, QString> EnvGismeteoIon::setupForecastConditionMappings(void) const
{
    QMap<QString, QString> forecastList;

    // Clouds + rain
    forecastList[QString::fromUtf8("d.sun.c1.png")]         = QString::fromUtf8("Ясно");
    forecastList[QString::fromUtf8("d.sun.c1.r1.png")]      = QString::fromUtf8("Ясно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c1.r2.png")]      = QString::fromUtf8("Ясно, дождь");
    forecastList[QString::fromUtf8("d.sun.c1.r3.png")]      = QString::fromUtf8("Ясно, ливень");
    forecastList[QString::fromUtf8("d.sun.c2.png")]         = QString::fromUtf8("Малооблачно");
    forecastList[QString::fromUtf8("d.sun.c2.r1.png")]      = QString::fromUtf8("Малооблачно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c2.r2.png")]      = QString::fromUtf8("Малооблачно, дождь");
    forecastList[QString::fromUtf8("d.sun.c2.r3.png")]      = QString::fromUtf8("Малооблачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c3.png")]         = QString::fromUtf8("Облачно");
    forecastList[QString::fromUtf8("d.sun.c3.r1.png")]      = QString::fromUtf8("Облачно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c3.r2.png")]      = QString::fromUtf8("Облачно, дождь");
    forecastList[QString::fromUtf8("d.sun.c3.r3.png")]      = QString::fromUtf8("Облачно, ливень");
    forecastList[QString::fromUtf8("d.sun.c4.png")]         = QString::fromUtf8("Пасмурно");
    forecastList[QString::fromUtf8("d.sun.c4.r1.png")]      = QString::fromUtf8("Пасмурно, небольшой дождь");
    forecastList[QString::fromUtf8("d.sun.c4.r2.png")]      = QString::fromUtf8("Пасмурно, дождь");
    forecastList[QString::fromUtf8("d.sun.c4.r3.png")]      = QString::fromUtf8("Пасмурно, ливень");

    // Clouds + rain + thunderstorm
    forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = QString::fromUtf8("Ясно, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r1.st.png")]   = QString::fromUtf8("Ясно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r2.st.png")]   = QString::fromUtf8("Ясно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.r3.st.png")]   = QString::fromUtf8("Ясно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = QString::fromUtf8("Малооблачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r2.st.png")]   = QString::fromUtf8("Малооблачно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.r3.st.png")]   = QString::fromUtf8("Малооблачно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = QString::fromUtf8("Облачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r1.st.png")]   = QString::fromUtf8("Облачно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r2.st.png")]   = QString::fromUtf8("Облачно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.r3.st.png")]   = QString::fromUtf8("Облачно, ливень, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = QString::fromUtf8("Пасмурно, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r1.st.png")]   = QString::fromUtf8("Пасмурно, небольшой дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r2.st.png")]   = QString::fromUtf8("Пасмурно, дождь, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.r3.st.png")]   = QString::fromUtf8("Пасмурно, ливень, гроза");

    // Clouds + snow
    //forecastList[QString::fromUtf8("d.sun.c1.png")]         = QString::fromUtf8("Ясно");
    forecastList[QString::fromUtf8("d.sun.c1.s1.png")]      = QString::fromUtf8("Ясно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c1.s2.png")]      = QString::fromUtf8("Ясно, снег");
    forecastList[QString::fromUtf8("d.sun.c1.s3.png")]      = QString::fromUtf8("Ясно, сильный снег");
    //forecastList[QString::fromUtf8("d.sun.c2.png")]         = QString::fromUtf8("Малооблачно");
    forecastList[QString::fromUtf8("d.sun.c2.s1.png")]      = QString::fromUtf8("Малооблачно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c2.s2.png")]      = QString::fromUtf8("Малооблачно, снег");
    forecastList[QString::fromUtf8("d.sun.c2.s3.png")]      = QString::fromUtf8("Малооблачно, сильный снег");
    //forecastList[QString::fromUtf8("d.sun.c3.png")]         = QString::fromUtf8("Облачно");
    forecastList[QString::fromUtf8("d.sun.c3.s1.png")]      = QString::fromUtf8("Облачно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c3.s2.png")]      = QString::fromUtf8("Облачно, снег");
    forecastList[QString::fromUtf8("d.sun.c3.s3.png")]      = QString::fromUtf8("Облачно, сильный снег");
    //forecastList[QString::fromUtf8("d.sun.c4.png")]         = QString::fromUtf8("Пасмурно");
    forecastList[QString::fromUtf8("d.sun.c4.s1.png")]      = QString::fromUtf8("Пасмурно, небольшой снег");
    forecastList[QString::fromUtf8("d.sun.c4.s2.png")]      = QString::fromUtf8("Пасмурно, снег");
    forecastList[QString::fromUtf8("d.sun.c4.s3.png")]      = QString::fromUtf8("Пасмурно, сильный снег");

    // Clouds + snow + thunderstorm
    //forecastList[QString::fromUtf8("d.sun.c1.st.png")]      = QString::fromUtf8("Ясно");
    forecastList[QString::fromUtf8("d.sun.c1.s1.st.png")]   = QString::fromUtf8("Ясно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s2.st.png")]   = QString::fromUtf8("Ясно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c1.s3.st.png")]   = QString::fromUtf8("Ясно, сильный снег, гроза");
    //forecastList[QString::fromUtf8("d.sun.c2.st.png")]      = QString::fromUtf8("Малооблачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s1.st.png")]   = QString::fromUtf8("Малооблачно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s2.st.png")]   = QString::fromUtf8("Малооблачно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c2.s3.st.png")]   = QString::fromUtf8("Малооблачно, сильный снег, гроза");
    //forecastList[QString::fromUtf8("d.sun.c3.st.png")]      = QString::fromUtf8("Облачно, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s1.st.png")]   = QString::fromUtf8("Облачно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s2.st.png")]   = QString::fromUtf8("Облачно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c3.s3.st.png")]   = QString::fromUtf8("Облачно, сильный снег, гроза");
    //forecastList[QString::fromUtf8("d.sun.c4.st.png")]      = QString::fromUtf8("Пасмурно, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s1.st.png")]   = QString::fromUtf8("Пасмурно, небольшой снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s2.st.png")]   = QString::fromUtf8("Пасмурно, снег, гроза");
    forecastList[QString::fromUtf8("d.sun.c4.s3.st.png")]   = QString::fromUtf8("Пасмурно, сильный снег, гроза");

    return forecastList;
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

QMap<QString, IonInterface::WindDirections> const& EnvGismeteoIon::windIcons(void) const
{
    static QMap<QString, WindDirections> const wval = setupWindIconMappings();
    return wval;
}

QStringList EnvGismeteoIon::validate(const QString& source) const
{
    kDebug() << "validate()";
    QStringList placeList;
    placeList.append(QString::fromUtf8("place|Москва|extra|4368"));
    return placeList;
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
        QStringList result = validate(sourceAction[2]);

        if (result.size() == 1) {
            setData(source, "validate", QString("gismeteo|valid|single|").append(result.join("|")));
            return true;
        } else if (result.size() > 1) {
            setData(source, "validate", QString("gismeteo|valid|multiple|").append(result.join("|")));
            return true;
        } else if (result.size() == 0) {
            setData(source, "validate", QString("gismeteo|invalid|single|").append(sourceAction[2]));
            return true;
        }

    } else if (sourceAction[1] == "weather" && sourceAction.size() > 2) {
        getXMLData(source);
        return true;
    } else {
        setData(source, "validate", "gismeteo|malformed");
        return true;
    }
    return false;
}

// Gets specific city XML data
void EnvGismeteoIon::getXMLData(const QString& source)
{
    foreach (const QString &fetching, m_jobList) {
        if (fetching == source) {
            // already getting this source and awaiting the data
            return;
        }
    }

    kDebug() << source;

    // Demunge source name for key only.
    QString dataKey = source;
    dataKey.remove("gismeteo|weather|");

    KUrl url = QString("http://www.gismeteo.ru/city/hourly/" + dataKey + "/");
    url = "file:///home/alex/Develop/kde/plasma-ion-gismeteo/4368.xml";
    kDebug() << "Will Try URL: " << url;

    KIO::TransferJob* const newJob  = KIO::get(url.url(), KIO::Reload, KIO::HideProgressInfo);

    m_jobXml.insert(newJob, QByteArray());
    m_jobList.insert(newJob, source);

    connect(newJob, SIGNAL(data(KIO::Job*,QByteArray)), this,
            SLOT(slotDataArrived(KIO::Job*,QByteArray)));
    connect(newJob, SIGNAL(result(KJob*)), this, SLOT(slotJobFinished(KJob*)));
}

void EnvGismeteoIon::slotDataArrived(KIO::Job *job, const QByteArray &data)
{

    if (data.isEmpty() || !m_jobXml.contains(job)) {
        return;
    }

    // Send to xml.
    m_jobXml[job].append(data);
}

void EnvGismeteoIon::slotJobFinished(KJob *job)
{
    // Dual use method, if we're fetching location data to parse we need to do this first
    const QString source = m_jobList.value(job);
    setData(source, Data());

    const QByteArray &data = m_jobXml.value(job);
    readXMLData(source, data);
    updateWeather(source);

    m_jobList.remove(job);
    m_jobXml.remove(job);
}

// Parse Weather data main loop, from here we have to decend into each tag pair
bool EnvGismeteoIon::readXMLData(const QString& source, const QByteArray& xml)
{
    WeatherData data;

    QXmlQuery query;

    kDebug() << "readXMLData()";

    // Setup model
    QLibXmlNodeModel model(query.namePool(), xml, QUrl());
    query.setFocus(model.dom());

    // Setup query
    QFile queryFile;
    queryFile.setFileName(KGlobal::dirs()->findResource("data", "plasma-ion-gismeteo/gismeteo.xq"));
    if (!queryFile.open(QIODevice::ReadOnly)) {
        kDebug() << "Can't open XQuery file" << PLASMA_ION_GISMETEO_SHARE_PATH "/gismeteo.xq";
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

    return false;
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
                .arg(forecast.day)
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

#include "ion_gismeteo.moc"
