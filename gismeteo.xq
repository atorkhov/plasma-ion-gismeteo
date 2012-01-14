for $elem in //div[@id='weather']/div/div
    return
        <current>
            <date> { data($elem/div[6]/span) } </date>
            <condition> { data($elem/dl/dd) } </condition>
            <temperature> { data($elem/div[2]) } </temperature>
            <pressure> { data($elem/div[3]) } </pressure>
            <wind>
                <direction> { data($elem/div[4]/dl/dt) } </direction>
                <speed> { data($elem/div[4]/dl/dd) } </speed>
            </wind>
            <humidity> { data($elem/div[5]) } </humidity>
            <waterTemp> { data(//div[@id='water']/div/div//h6/span) } </waterTemp>
        </current> ,
for $elem in //div[@id='astronomy']/div/div/div[2]
    return
        <astronomy>
            <sunrise> { data($elem/ul[1]/li[1]) } </sunrise>
            <sunset>  { data($elem/ul[1]/li[2]) } </sunset>
            <moonPhase> { data($elem/ul[2]/li[1]) } </moonPhase>
        </astronomy> ,
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
            <comfort_temp> { data($row/td[7]) } </comfort_temp>
        </forecast>
