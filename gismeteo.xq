for $elem in //div[@id='weather']/div/div
    return
        <current>
            <date> { data($elem/div[6]/span) } </date>
            <condition> { data($elem/dl/dd) } </condition>
            <conditionIcon> { substring-before(substring-after(data($elem/dl/dt/@style), 'new/'), ')') } </conditionIcon>
            <temperature> { data($elem/div[2]) } </temperature>
            <pressure> { data($elem/div[3]) } </pressure>
            <windDirection> { data($elem/div[4]/dl/dt) } </windDirection>
            <windSpeed> { data($elem/div[4]/dl/dd) } </windSpeed>
            <humidity> { data($elem/div[5]) } </humidity>
            <waterTemperature> { data(//div[@id='water']/div/div//h6/span) } </waterTemperature>
        </current> ,
for $elem in //div[@id='astronomy']/div/div/div[2]
    return
        <astronomy>
            <sunrise> { data($elem/ul[1]/li[1]) } </sunrise>
            <sunset>  { data($elem/ul[1]/li[2]) } </sunset>
            <moonPhase> { data($elem/ul[2]/li[1]) } </moonPhase>
        </astronomy> ,
for $day in //div[@id='weather-daily']/div/div[2]/div
    return
        <forecast>
            <day> { data($day/dl/dt) } </day>
            <icon> { substring-after(data($day/img/@src), 'new/') } </icon>
            <temperature> { data($day/div) } </temperature>
        </forecast> ,
<!--
for $row in //div[@id='weather-daily']/div/div/table/tbody/tr
    return
        <forecast>
            { $row/@id }
            <date> { substring-after(data($row/th/@title), 'Local: ') } </date>
            <condition> { data($row/td[2]) } </condition>
            <temperature> { data($row/td[3]) } </temperature>
            <pressure> { data($row/td[4]) } </pressure>
            <windDirection> { data($row/td[5]/dl/dt) } </windDirection>
            <windSpeed> { data($row/td[5]/dl/dd) } </windSpeed>
            <humidity> { data($row/td[6]) } </humidity>
            <comfortTemperature> { data($row/td[7]) } </comfortTemperature>
        </forecast>
-->