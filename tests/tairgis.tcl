set testmodule [file normalize yourpath/tairgis.so]

start_server {tags {"ex_gis"} overrides {bind 0.0.0.0}} {
    r module load $testmodule

    set area hangzhou
    set polygon_name alibaba-xixi-campus
    set polygon_wkt "POLYGON((30 10,40 40,20 40,10 20,30 10))"
    set point_wkt "POINT (30 11)"

    test {gis.add/gis.search} {
        set ex_area zhenzhen
        set ex_polygonname alizhongxin
        set ex_polygon_wkt "POLYGON ((10 10,10 20,20 20))"
        set ex_point_wkt "POINT (16 16)"
        set ex_linestring_wkt1 "LINESTRING (10 10, 15 15)"
        set ex_polygon_wkt1 "POLYGON ((15 15, 16 17, 12 19))"

        set new_field [r gis.add $ex_area $ex_polygonname $ex_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.search $ex_area $ex_point_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        set res [r gis.search $ex_area $ex_linestring_wkt1]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname


        set res [r gis.search $ex_area $ex_polygon_wkt1]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        r del $ex_area
    }

    test {gis.add/gis.contains} {
        set con_area beijing
        set con_polygonname tengxun
        set con_polygon_wkt "POLYGON ((10 10, 10 50, 50 50, 50 10))"
        set con_point_wkt "POINT (15 15)"
        set con_linestring_wkt "LINESTRING (20 20, 30 30)"
        set con_polygon_wkt "POLYGON ((15 15, 15 30, 30 30, 30 15))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area $con_point_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $con_polygonname

        set res [r gis.contains $con_area $con_linestring_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $con_polygonname

        set res [r gis.contains $con_area $con_polygon_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $con_polygonname

        r del $con_area
    }

    test {gis.add/gis.contains withoutwkt} {
        set con_area beijing
        set con_polygonname tengxun
        set con_polygon_wkt "POLYGON((10 10,10 50,50 50,50 10))"
        set con_point_wkt "POINT (15 15)"

        set ex_polygonname alizhongxin
        set ex_polygon_wkt "POLYGON((10 10,10 20,20 20))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field
        set new_field [r gis.add $con_area $ex_polygonname $ex_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area $con_point_wkt]
        set k [lindex $res 1]
        assert_equal [lindex $k 0] $con_polygonname
        assert_equal [lindex $k 1] $con_polygon_wkt
        assert_equal [lindex $k 2] $ex_polygonname
        assert_equal [lindex $k 3] $ex_polygon_wkt

        set res [r gis.contains $con_area $con_point_wkt WITHOUTWKT]
        set k [lindex $res 1]
        assert_equal [lindex $k 0] $con_polygonname
        assert_equal [lindex $k 1] $ex_polygonname

        r del $con_area
    }

    # https://work.aone.alibaba-inc.com/issue/36740612
    test {gis.add/gis.contains when point in polygon but parallel with some points or edges -1} {
        set con_area chengdu
        set con_polygonname test
        set con_polygon_wkt "POLYGON((10 0,20 10,10 20,0 10,10 0))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area "POINT (5 20)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 4)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (0 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (10 10)"]
        assert_equal 1 [lindex $res 0]

        r del $con_area
    }

    test {gis.add/gis.contains when point in polygon but parallel with some points or edges -2} {
        set con_area chengdu
        set con_polygonname test
        set con_polygon_wkt "POLYGON((0 0, 30 0, 30 10, 20 20, 15 15, 10 10, 10 30, 0 30,0 0))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area "POINT (10 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (10 20)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (15 16)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (15 14)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 5)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 20)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (-5 30)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (30 12)"]
        assert_equal 0 [lindex $res 0]

        r del $con_area
    }

    test {gis.add/gis.contains when point in polygon but parallel with some points or edges -3} {
        set con_area chengdu
        set con_polygonname test
        set con_polygon_wkt "POLYGON((0 0, 20 20, 30 10, 40 10,40 20, 50 20, 50 0, 0 0))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area "POINT (20 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (20 5)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (30 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (10 20)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (45 10)"]
        assert_equal 1 [lindex $res 0]

        r del $con_area
    }

    test {gis.add/gis.contains when point in polygon but parallel with some points or edges -4} {
        set con_area chengdu
        set con_polygonname test
        set con_polygon_wkt "POLYGON((0 0, 10 20, 20 20, 30 30, 30 10, 40 5, 40 0, 0 0))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area "POINT (20 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 20)"]
        assert_equal 0 [lindex $res 0]

        r del $con_area
    }

    test {gis.add/gis.contains when point in polygon but parallel with some points or edges -5} {
        set con_area chengdu
        set con_polygonname test
        set con_polygon_wkt "POLYGON((0 0, 0 10, 5 15, 10 10, 20 10, 20 25, 5 25, 5 30, 25 30, 25 20, 25 0, 0 0))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area "POINT (0 10)"]
        assert_equal 1 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (0 15)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (0 25)"]
        assert_equal 0 [lindex $res 0]

        set res [r gis.contains $con_area "POINT (5 17)"]
        assert_equal 0 [lindex $res 0]

        r del $con_area
    }

    test {exgetall withoutwkt} {
        set con_area beijing
        set con_polygonname tengxun
        set con_polygon_wkt "POLYGON((10 10,10 50,50 50,50 10))"
        set con_point_wkt "POINT (15 15)"

        set ex_polygonname alizhongxin
        set ex_polygon_wkt "POLYGON((10 10,10 20,20 20))"

        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field
        set new_field [r gis.add $con_area $ex_polygonname $ex_polygon_wkt]
        assert_equal 1 $new_field

        set k [r gis.getall $con_area]
        assert_equal [lindex $k 0] $ex_polygonname
        assert_equal [lindex $k 1] $ex_polygon_wkt
        assert_equal [lindex $k 2] $con_polygonname
        assert_equal [lindex $k 3] $con_polygon_wkt

        set k [r gis.getall $con_area WITHOUTWKT]
        assert_equal [lindex $k 0] $ex_polygonname
        assert_equal [lindex $k 1] $con_polygonname

        r del $con_area
    }

    test {gis.add/gis.intersects } {
        set ex_area zhongshan
        set ex_polygonname lisi
        set ex_polygon_wkt "POLYGON ((10 10,10 50,50 50, 50 10))"
        set ex_point_wkt "POINT (30 30)"
        set ex_linestring_wkt1 "LINESTRING (20 20, 30 30)"
        set ex_linestring_wkt2 "LINESTRING (5 5, 30 30)"
        set ex_polygon_wkt1 "POLYGON ((15 15, 15 30, 30 30, 30 15))"
        set ex_polygon_wkt2 "POLYGON ((5 5, 5 30, 30 30, 30 5))"

        set new_field [r gis.add $ex_area $ex_polygonname $ex_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.intersects $ex_area $ex_point_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        set res [r gis.intersects $ex_area $ex_linestring_wkt1]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        set res [r gis.intersects $ex_area $ex_linestring_wkt2]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        set res [r gis.intersects $ex_area $ex_polygon_wkt1]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        set res [r gis.intersects $ex_area $ex_polygon_wkt2]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $ex_polygonname

        r del $ex_area
    }

    test {exgis rdb} {
        set new_field [r gis.add $area $polygon_name $polygon_wkt]
        assert_equal 1 $new_field

        r bgsave
        waitForBgsave r
        r debug reload

        set res [r gis.get $area $polygon_name]
        assert_equal $res $polygon_wkt

        r del $area
    }

    test {exgis aof} {
        r config set aof-use-rdb-preamble no
        set new_field [r gis.add $area polygon1 $polygon_wkt]
        assert_equal 1 $new_field

        set new_field [r gis.add $area polygon2 $polygon_wkt]
        assert_equal 1 $new_field

        r bgrewriteaof
        waitForBgrewriteaof r
        r debug loadaof

        set res1 [r gis.get $area polygon1]
        set res2 [r gis.get $area polygon2]
        assert_equal $res1 $polygon_wkt
        assert_equal $res2 $polygon_wkt
        assert_equal $res1 $res2

        r del $area
    }

    test {exgis type} {
        set new_field [r gis.add $area $polygon_name $polygon_wkt]
        assert_equal 1 $new_field

        set res [r type $area]
        assert_equal $res "exgistype"

        r del $area
    }

    test {gis.del} {
        set new_field [r gis.add $area polygon1 $polygon_wkt]
        assert_equal 1 $new_field

        set new_field [r gis.add $area polygon2 $polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.get $area polygon1]
        assert_equal $res $polygon_wkt

        assert_equal OK [r gis.del $area polygon1]

        assert_equal "" [r gis.get $area polygon1]

        set res [r gis.get $area polygon2]
        assert_equal $res $polygon_wkt

        r del $area
    }

    test {dump/restore} {
        set new_field [r gis.add $area polygon1 $polygon_wkt]
        assert_equal 1 $new_field

        set dump [r dump $area]
        r del $area

        assert_equal "OK" [r restore $area 0 $dump]
        assert_equal $polygon_wkt [r gis.get $area polygon1]
    }

    test {gis.contains multipolygon} {
        set con_area beijing
        set con_polygonname tengxun
        set con_polygon_wkt "MULTIPOLYGON ( ((30 10, 40 40, 20 40, 10 20, 30 10)), ((30 10, 40 10, 40 0)) )"
        set con_point_wkt "POINT (30 10)"

        r del $con_area
        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area $con_point_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $con_polygonname
    }

    test {gis.contains multipoint} {
        set con_area beijing
        set con_polygonname tengxun
        set con_polygon_wkt "MULTIPOINT (30 10, 40 40, 20 40, 10 20, 30 10)"
        set con_point_wkt "POINT (30 10)"

        r del $con_area
        set new_field [r gis.add $con_area $con_polygonname $con_polygon_wkt]
        assert_equal 1 $new_field

        set res [r gis.contains $con_area $con_point_wkt]
        set cur [lindex $res 0]
        set k [lindex $res 1]
        set name [lindex $k 0]
        assert_equal $name $con_polygonname
    }

    test {gis.intersects search linestring} {
        r del sect

        assert_equal 1 [r gis.add sect point "POINT (20 20)"]
        assert_equal 1 [r gis.add sect line "linestring (0 20, 40 20)"]
        assert_equal 1 [r gis.add sect polygon "polygon ((10 10, 30 10, 30 30, 10 30))"]

        set res [r gis.intersects sect "linestring (20 0,20 40)"]
        set cur [lindex $res 0]
        assert_equal 3 $cur

        r del sect
    }
}

start_server {tags {"ex_gis"} overrides {bind 0.0.0.0}} {
    r module load $testmodule

    set slave [srv 0 client]
    set slave_host [srv 0 host]
    set slave_port [srv 0 port]
    set slave_log [srv 0 stdout]

    start_server {tags {"ex_gis"} overrides {bind 0.0.0.0}} {
        r module load $testmodule

        set master [srv 0 client]
        set master_host [srv 0 host]
        set master_port [srv 0 port]

        $slave slaveof $master_host $master_port

        wait_for_condition 50 100 {
            [lindex [$slave role] 0] eq {slave} &&
            [string match {*master_link_status:up*} [$slave info replication]]
        } else {
            fail "Can't turn the instance into a replica"
        }

        test {exgset/gis.get master-slave} {
            set new_field [$master gis.add $area $polygon_name $polygon_wkt]
            assert_equal 1 $new_field

            $master WAIT 1 5000

            set res [$slave gis.get $area $polygon_name]
            assert_equal $res $polygon_wkt

            $master del $area
        }

        test {expire master-slave} {
            set new_field [$master gis.add $area $polygon_name $polygon_wkt]
            assert_equal 1 $new_field

            $master WAIT 1 5000

            assert_equal $polygon_wkt [$slave gis.get $area $polygon_name]

            $master expire $area 1

            after 2000

            assert_equal "" [$slave gis.get $area $polygon_name]

            $master del $area
        }

        test {gis.del master-slave} {
            set new_field [$master gis.add $area $polygon_name $polygon_wkt]
            assert_equal 1 $new_field

            $master WAIT 1 5000

            assert_equal $polygon_wkt [$slave gis.get $area $polygon_name]

            assert_equal OK [$master gis.del $area $polygon_name]

            $master WAIT 1 5000

            assert_equal 0 [$slave exists $area]
            assert_equal "" [$slave gis.get $area $polygon_name]

            $master del $area
        }

        test {rdb master-slave} {
            set new_field [$master gis.add $area $polygon_name $polygon_wkt]
            assert_equal 1 $new_field

            $master bgsave
            waitForBgsave $master
            $master debug reload

            set res [$slave gis.get $area $polygon_name]
            assert_equal $res $polygon_wkt

            $master del $area
        }

        test {aof master-slave} {
            $master config set aof-use-rdb-preamble no
            $slave config set aof-use-rdb-preamble no

            set new_field [$master gis.add $area polygon1 $polygon_wkt]
            assert_equal 1 $new_field

            set new_field [$master gis.add $area polygon2 $polygon_wkt]
            assert_equal 1 $new_field

            $master bgrewriteaof
            waitForBgrewriteaof $master
            $master debug loadaof

            set res1 [$slave gis.get $area polygon1]
            set res2 [$slave gis.get $area polygon2]
            assert_equal $res1 $polygon_wkt
            assert_equal $res2 $polygon_wkt
            assert_equal $res1 $res2

            $master del $area
        }
    }
}

start_server {tags {"gis-geo"}} {
    r module load $testmodule

    test {gis.add create} {
        r gis.add nyc "lic market" "POINT (-73.9454966 40.747533)"
    } {1}

    test {gis.add update} {
        r gis.add nyc "lic market" "POINT (-73.9454966 40.747533)"
    } {1}

    test {gis.add multi add} {
        r gis.add nyc "central park n/q/r" "POINT (-73.9733487 40.7648057)" "union square" "POINT (-73.9903085 40.7362513)" "wtc one" "POINT (-74.0131604 40.7126674)" "jfk" "POINT (-73.7858139 40.6428986)" "q4" "POINT (-73.9375699 40.7498929)" "4545" "POINT (-73.9564142 40.7480973)"
    } {6}

    test {gis.search simple (sorted)} {
        r gis.search nyc radius -73.9798091 40.7598464 3 km asc withoutvalue
    } {3 {{central park n/q/r} 4545 {union square}}}

    test {gis.search withdist (sorted)} {
        r gis.search nyc radius -73.9798091 40.7598464 3 km asc withdist withoutvalue
    } {3 {{central park n/q/r} 0.7749 4545 2.3650 {union square} 2.7695}}

    test {gis.search with COUNT} {
        r gis.search nyc radius -73.9798091 40.7598464 10 km count 3 withoutvalue
    } {3 {{central park n/q/r} 4545 {union square}}}

    test {gis.search with COUNT but missing integer argument} {
        catch {r gis.search nyc radius -73.9798091 40.7598464 10 km count} e
        set e
    } {ERR*count need number*}

    test {gis.search with COUNT DESC} {
        r gis.search nyc radius -73.9798091 40.7598464 10 km count 2 desc  withoutvalue
    } {2 {{wtc one} q4}}

    test {gis.search HUGE, issue #2767} {
        r gis.add users user_000000 "POINT (-47.271613776683807 -54.534504198047678)"
        r gis.search users radius 0 0  50000 km withoutvalue
    } {1 user_000000}

    test {gis.search by member simple (sorted)} {
        r gis.search nyc member "wtc one" 7 km asc withoutvalue
    } {5 {{wtc one} {union square} 4545 {central park n/q/r} {lic market}}}

    test {gis.search by member withdist (sorted)} {
        r gis.search nyc member "wtc one" 7 km asc withdist withoutvalue
    } {5 {{wtc one} 0.0000 {union square} 3.2544 4545 6.1972 {central park n/q/r} 6.6998 {lic market} 6.8967}}
}

