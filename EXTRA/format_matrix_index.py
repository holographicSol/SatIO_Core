#### Add Items ####
x = [
    "NONE", #0
    "ON", #1
    "Switch Link", #2
    "Time HHMMSS", #3
    "Week Day", #4
    "Month Day", #5
    "Month", #6
    "Year", #7
    "SatIO Deg Lat", #8
    "SatIO Deg Lon", #9
    "SatIO INS Lat", #10
    "SatIO INS Lon", #11
    "SatIO INS Heading", #12
    "SatIO INS Alt", #13
    "GNGGA Status", #14
    "GNGGA Sat Count", #15
    "GNGGA Prescion", #16
    "GNGGA Altitude", #17
    "GNRMC Ground Speed", #18
    "GNRMC Heading", #19
    "GPATT Line", #20
    "GPATT Static", #21
    "GPATT Run State", #22
    "GPATT INS", #23
    "GPATT Mileage", #24
    "GPATT GST", #25
    "GPATT Yaw", #26
    "GPATT Roll", #27
    "GPATT Pitch", #28
    "GNGGA Valid CS", #29
    "GNRMC Valid CS", #30
    "GPATT Valid CS", #31
    "GNGGA Valid CD", #32
    "GNRMC Valid CD", #33
    "GPATT Valid CD", #34
    "GNRMC Pos Stat A", #35
    "GNRMC Pos Stat V", #36
    "GNRMC Mode Ind A", #37
    "GNRMC Mode Ind D", #38
    "GNRMC Mode Ind E", #39
    "GNRMC Mode Ind N", #40
    "GNRMC Hemi North", #41
    "GNRMC Hemi South", #42
    "GNRMC Hemi East", #43
    "GNRMC Hemi West", #44
    "G0 G-Force X", #45
    "G0 G-Force Y", #46
    "G0 G-Force Z", #47
    "G0 Incline X", #48
    "G0 Incline Y", #49
    "G0 Incline Z", #50
    "G0 Mag Field X", #51
    "G0 Mag Field Y", #52
    "G0 Mag Field Z", #53
    "G0 Velocity X", #54
    "G0 Velocity Y", #55
    "G0 Velocity Z", #56
    "Meteor", #57
    "Sun Azimuth", #58
    "Sun Altitude", #59
    "Sun Helio Ecl Lat", #60
    "Sun Helio Ecl Lon", #61
    "Luna Azimuth", #62
    "Luna Altitude", #63
    "Luna Phase", #64
    "Mercury Azimuth", #65
    "Mercury Altitude", #66
    "Mercury H.Ecliptic Lat", #67
    "Mercury H.Ecliptic Lon", #68
    "Mercury Ecliptic Lat", #69
    "Mercury Ecliptic Lon", #70
    "Venus Azimuth", #71
    "Venus Altitude", #72
    "Venus H.Ecliptic Lat", #73
    "Venus H.Ecliptic Lon", #74
    "Venus Ecliptic Lat", #75
    "Venus Ecliptic Lon", #76
    "Earth Ecliptic Lon", #77
    "Mars Azimuth", #78
    "Mars Altitude", #79
    "Mars H.Ecliptic Lat", #80
    "Mars H.Ecliptic Lon", #81
    "Mars Ecliptic Lat", #82
    "Mars Ecliptic Lon", #83
    "Jupiter Azimuth", #84
    "jupiter Altitude", #85
    "Jupiter H.Ecliptic Lat", #86
    "Jupiter H.Ecliptic Lon", #87
    "Jupiter Ecliptic Lat", #88
    "Jupiter Ecliptic Lon", #89
    "Saturn Azimuth", #90
    "Saturn Altitude", #91
    "Saturn H.Ecliptic Lat", #94
    "Saturn H.Ecliptic Lon", #95
    "Saturn Ecliptic Lat", #96
    "Saturn Ecliptic Lon", #97
    "Uranus Azimuth", #98
    "Uranus Altitude", #99
    "Uranus H.Ecliptic Lat", #100
    "Uranus H.Ecliptic Lon", #101
    "Uranus Ecliptic Lat", #102
    "Uranus Ecliptic Lon", #103
    "Neptune Azimuth", #104
    "Neptune Altitude", #105
    "Neptune H.Ecliptic Lat", #106
    "Neptune H.Ecliptic Lon", #107
    "Neptune Ecliptic Lat", #108
    "Neptune Ecliptic Lon", #109
    "AD Multiplexer 0", #110
    "Map Slot", #111
    "SD Card Inserted", #112
    "SD Card Mounted", #113
    "Port Con 0", #114
    ]

#### Copy Output Into SatIO Program Files and README ####

# DEFINES
print("\n\n")
i = 0
for _ in x:
    print("#define INDEX_MATRIX_SWITCH_FUNCTION_" + _.upper().replace(" ", "_").replace(".", "_").replace("-", "_") + " " + str(i))
    i+=1

# New C List
print("\n\n")
i = 0
for _ in x:
    print('"' + str(_) + '", //' + str(i))
    i+=1

# New C switch statement: Return input value
print("\n\n")
i = 0
print("switch (index) {")
for _ in x:
    print("    case INDEX_MATRIX_SWITCH_FUNCTION_" + _.upper().replace(" ", "_").replace(".", "_").replace("-", "_") + ":")
    print("        result = String();")
    print("        break;")
    i+=1
print("    default: result = String(""); break;
print("}")

# README
print("\n\n")
i = 0
for _ in x:
    print('[' + str(i) + '] ' +  str(_))
    i+=1

# New Python List
print("\n\n")
i = 0
for _ in x:
    print('"' + str(_) + '", #' + str(i))
    i+=1





    
