import os
import sys
from django.shortcuts import render, redirect, reverse
from django.http import HttpResponse
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from datetime import datetime, timezone, timedelta
import pytz
import subprocess
import logging
import glob
import pandas as pnd

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)
console_handler = logging.StreamHandler()
logger.addHandler(console_handler)

#---------------------------------------
def formatted_ocean(chaine):
    if chaine.endswith(' OCEAN'):
        return chaine[:-6]
    
    return chaine

def formatted_wind(chaine):

    if isinstance(chaine, str):
        chaine = chaine.lower() == 'true'
    
    if chaine:
        return 'OFF-SHORE'
    else:
        return 'NONE'
    
def format_date(date_time):

    date_format = "%a %B %d %Y %H:%M"
    return (date_time.strftime(date_format))

def formatted_country(country_name, country_code):

    if(country_code == 'NAM'):
        country_code = 'NA'

    country_s = f'<img src="https://flagsapi.com/{country_code}/shiny/24.png">' + ' ' + country_name
    return (country_s)

def formatted_wave(wave_meter_m):
    return (f"{wave_meter_m} m")

def formatted_period(wave_period_s):
    return (f"{wave_period_s} s")

def make_clickable(chaine, lat, lon):
    url = f"https://www.google.com/maps?q={lat},{lon}"
    
    link = f'<a href="{url}" target="_blank" ' \
           f'style="color: black; ' \
           f'text-align: center; ' \
           f'font-weight: bold; ' \
           f'font-family: Courier New, Courier, monospace;">' \
           f'{chaine}</a>'
    
    return link

#---------------------------------------
def index(request):

    if request.method == 'POST':
        return redirect("surfspots_viewer")

    return render(request, 'surfspots_index.html')


#---------------------------------------------------
def surfspots_viewer(request):

    directory_path = '/home/user/VORONIZING/surfspots/script/'

    INDEX_file_path = directory_path + 'SURFSPOTS_RUN_INDEX.csv'
    TOP_file_path = ''
    
    datetime_table = pnd.read_csv(INDEX_file_path, header=None)
    date_format = "%Y_%m_%d_%H_%M_%S"

    nb_prediction = 14
    time_step = 6

    prediction_table_array = []

    current_datetime = datetime.now(timezone.utc)

    selected_datetime = None
    min_diff_s = sys.float_info.max
    #-------------------------------------------
    for index, row in datetime_table.iterrows():

            dt_s = row[0]
            dt = datetime.strptime(dt_s, date_format)
            dt_utc = dt.replace(tzinfo=pytz.UTC)

            diff = abs(current_datetime - dt_utc)
            diff_s = diff.total_seconds()

            if (diff_s < min_diff_s):
                min_diff_s = diff_s
                selected_datetime = dt
    
    dt1 = selected_datetime

    #-----------------------------
    for i in range(nb_prediction):

        dt1_s = datetime.strftime(dt1, date_format)

        TOP_file_path = directory_path + 'SURFSPOTS_TOP_' + dt1_s + '.csv'

        if os.path.isfile(TOP_file_path):

            selected_columns = ['NAME', 'CITY', 'COUNTRY', 'COUNTRY_CODE', 'WAVE_HEIGHT_METERS', 'WAVE_PERIOD_SECONDS', 'WIND_OFFSHORE', 'RATING', 'OCEAN', 'LEVEL', 'VALID_TIME_LOCAL', 'SPOT_LATITUDE_180', 'SPOT_LONGITUDE_180']
            html_output_file_path = '/home/user/VORONIZING/surfspots/templates/surfspots_viewer.html'

            df = pnd.read_csv(TOP_file_path, usecols=selected_columns)

            df['WAVE_HEIGHT_METERS'] = df['WAVE_HEIGHT_METERS'].round(1)
            df['WAVE_PERIOD_SECONDS'] = df['WAVE_PERIOD_SECONDS'].astype(int)

            df['NAME'] = df.apply(lambda row: make_clickable(row['NAME'], row['SPOT_LATITUDE_180'], row['SPOT_LONGITUDE_180']), axis=1)
            df['COUNTRY'] = df.apply(lambda row: formatted_country(row['COUNTRY'], row['COUNTRY_CODE']), axis=1)
            df['RATING'] = df['RATING'].apply(lambda x: '*' * int(x))

            df['WIND_OFFSHORE'] = df['WIND_OFFSHORE'].apply(formatted_wind)
            df['OCEAN'] = df['OCEAN'].apply(formatted_ocean)
            df['WAVE_HEIGHT_METERS'] = df['WAVE_HEIGHT_METERS'].apply(formatted_wave)
            df['WAVE_PERIOD_SECONDS'] = df['WAVE_PERIOD_SECONDS'].apply(formatted_period)

            df['VALID_TIME_LOCAL'] =  pnd.to_datetime(df['VALID_TIME_LOCAL']).apply(format_date)

            df.drop(columns=['SPOT_LATITUDE_180', 'SPOT_LONGITUDE_180'], inplace=True)
            df.drop(columns=['COUNTRY_CODE'], inplace=True)

            new_header = ['SPOT NAME', 'LOCAL TIME', 'CITY', 'COUNTRY', 'WAVE HEIGHT', 'WAVE PERIOD', 'WIND TYPE', 'RATING', 'OCEAN', 'LEVEL']
            ordered_columns = ['NAME', 'VALID_TIME_LOCAL', 'CITY', 'COUNTRY', 'WAVE_HEIGHT_METERS', 'WAVE_PERIOD_SECONDS', 'WIND_OFFSHORE', 'RATING', 'OCEAN', 'LEVEL']
            df = df[ordered_columns]

            df_sorted = df.sort_values(by='RATING', ascending=False)
            df_sorted.columns = new_header

            prediction_table_i = df_sorted.to_html(index=False, classes='table align-middle table-striped table-borderless custom-table', header=new_header, justify='left', escape=False)
            
            prediction_table_array.append((dt1_s, prediction_table_i))

            dt1 += timedelta(hours=(time_step))

    #----- CREATION OF HTML FILE -----#
    html_content = f"""

        {{% load static %}}

        <!DOCTYPE html>

        <html>

            <head>
                <title>Surf Spots</title>

                <link rel="shortcut icon" type="image/png" href="{{% static 'css/wave_icon.svg' %}}"/>

                <link rel="stylesheet" href=" {{% static 'bootstrap/css/bootstrap.css' %}} ">

                <script src=" {{% static 'bootstrap/js/bootstrap.js' %}} "></script>
                <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.6.4/jquery.min.js"></script>
                <script src="{{% static 'js/client_time.js' %}}"></script>

                 <style>
                    .custom-table th {{
                        background-color: #2E86C1;
                        color: white;
                    }}
                </style>
            </head>

            <body class="bg-light">

                {{% csrf_token %}}

                <header class="container-fluid bg-light mt-4">
                    <h1 style="color: #00288B;
                            text-align: center;
                            font-weight: bold;
                            font-family: 'Courier New', Courier, monospace;"> 

                        Surf Spots
                    </h1>
                </header>
"""
    #----------------------------------
    for item in prediction_table_array:
        
        printable_date_format = "%B %d %Y %H:%M"

        prediction_date_obj = datetime.strptime(item[0], date_format)
        formatted_date = prediction_date_obj.strftime(printable_date_format)

        html_content += f"""
        <div class="container-fluid mt-4 ml-2"
             style="color: white;
                    text-align: center;
                    font-size: x-large;
                    font-family: 'Courier New', Courier, monospace;
                    font-weight: bold;
                    background-color: #00288B">
            {formatted_date} UTC
        </div>
        
        <div class="container-fluid table-responsive bg-light mt-4 ml-2"
            style="text-align: left;
                   font-weight: bold;
                   font-family: 'Courier New', Courier, monospace;">
            {item[1]}
        </div>
    """

    html_content += """ 
            </body>\n
        </html>
    """
    
    with open(html_output_file_path, 'w', encoding='utf-8') as f:
        f.write(html_content)

    return render(request, 'surfspots_viewer.html')


