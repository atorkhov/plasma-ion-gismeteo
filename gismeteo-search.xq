for $city in //div[@class='cities_by_name']/div/div/ul/li
    return
        <place>
            <name> { data($city/a[1]) } </name>
            <link> { data($city/a[1]/@href) } </link>
        </place>
