#
# The MIT License (MIT)
#
# Copyright (c) 2026 Scott Moreau <oreaus@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

#
# OpenWeatherMap Free API Key Signup
#
# 1) Navigate to https://home.openweathermap.org/users/sign_up
#
# 2) Fill out the form
#
# 3) Complete the captcha
#
# 4) Click Create Account button
#
# 5) Check email and verify account
#
# 6) Wait about 2 hours for the key to become active
#
# 7) Navigate to https://home.openweathermap.org/api_keys
#
# 8) Copy the Default API key for use
#
# 9) Run this script with your API key and location
#
#
# Examples:
#
# $ python weather.py --help
#
# usage: Weather [-h] [-l LOCATION] [-k APIKEY] [-m]
#
# Get weather data with icon.
#
# options:
#   -h, --help            show this help message and exit
#   -l, --location LOCATION
#   -k, --apikey APIKEY
#   -c, --classic-icons
#   -m, --metric
#   -d, --debug
#
# Copyright (c) 2026 Scott Moreau <oreaus@gmail.com>
#
# $ python weather.py -k 8b0017275348eaf1a674045c86dadd32 -l 80918
#
# Weather information for Pikeview, US: 15°F - light snow
#
# $ python weather.py -k 8b0017275348eaf1a674045c86dadd32 -l London -m
#
# Weather information for London, GB: 5°C - overcast clouds
#
# Writes ~/.local/share/weather/data/data.json in the following format:
#
# {
#     "temp": "5\u00b0C",
#     "conditions": "Clouds",
#     "icon": "/home/user/.local/share/weather/icons/04n@2x.png"
# }
#
# Applications can then read this file to get the current weather and icon
#
# This script is intended to be run periodically in the background:
#
# while true; do python weather.py [options]; sleep 10m; done
#


from datetime import datetime
from pathlib import Path
import requests
import argparse
import json
import sys
import os

icon_map = {
    "01d": "1",  # clear sky day
    "01n": "33", # clear sky night
    "02d": "2",  # few clouds day
    "02n": "34", # few clouds night
    "03d": "3",  # scattered clouds day
    "03n": "35", # scattered clouds night
    "04d": "4",  # broken clouds day
    "04n": "36", # broken clouds night
    "09d": "14", # shower rain day
    "09n": "39", # shower rain night
    "10d": "13", # rain day
    "10n": "40", # rain night
    "11d": "16", # thunderstorm day
    "11n": "42", # thunderstorm night
    "13d": "23", # snow day
    "13n": "44", # snow night
    "50d": "5",  # mist day
    "50n": "37", # mist night
}

weather = {}

def get_weather_info():
    if weather["api_key"] is None:
        print("Set OpenWeatherMap api key to enable weather updates.")
        return
    if weather["location_key"] is None:
        print("Set OpenWeatherMap location to get localized weather updates.")
        return

    current_time = datetime.now().time()
    formatted_time = current_time.strftime("%l:%M:%S")
    print(formatted_time, "- Retrieving weather information..")

    try:
        if weather["metric_units"] is True:
            units = "metric"
        else:
            units = "imperial"
        weather_data_url = "http://api.openweathermap.org/data/2.5/weather?q=" + str(weather["location_key"]) + "&units=" + units + "&appid=" + str(weather["api_key"])
        weather_data = json.loads(requests.get(weather_data_url).content)
        if weather["debug"]:
            print(weather_data)
        if weather["metric_units"] is True:
            weather["temperature"] = str(int(weather_data["main"]["temp"])) + "°C"
        else:
            weather["temperature"] = str(int(weather_data["main"]["temp"])) + "°F"
        weather_icon_code = weather_data["weather"][0]["icon"]
        if weather["classic-icons"]:
            weather_icon_name = weather_icon_code + "@2x.png"
            weather_icon_url = "https://openweathermap.org/img/wn/" + weather_icon_name
        else:
            weather_icon_name = icon_map[weather_icon_code] + ".svg"
            weather_icon_url = "https://www.accuweather.com/assets/images/weather-icons/v2a/" + weather_icon_name
        weather_icon_path = weather["icon_directory"] + "/" + weather_icon_name
        print(f"Checking for icon {weather_icon_path}")
        if not os.path.exists(weather_icon_path):
            img_data = requests.get(weather_icon_url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36'}).content
            with open(weather_icon_path, 'wb') as weather_icon:
                weather_icon.write(img_data)
        print(f"Weather information for {weather_data["name"]}, {weather_data["sys"]["country"]}: {weather["temperature"]} - {weather_data["weather"][0]["description"]}")
        data = {
            "temp": weather["temperature"],
            "conditions": weather_data["weather"][0]["main"],
            "icon": weather_icon_path
        }
        with open(weather["data_directory"] + "/" + "data.json", "w") as file:
            json.dump(data, file, indent=4)

    except Exception as e:
        print("Failed to update weather:", e)
        exit(-1)

def main():
    parser = argparse.ArgumentParser(
        prog="Weather",
        description="Get weather data with icon.",
        epilog="Copyright (c) 2026 Scott Moreau <oreaus@gmail.com>")
    parser.add_argument("-l", "--location")
    parser.add_argument("-k", "--apikey")
    parser.add_argument("-c", "--classic-icons", action="store_true")
    parser.add_argument("-m", "--metric", action="store_true")
    parser.add_argument("-d", "--debug", action="store_true")
    args = parser.parse_args()
    if args.apikey is None:
        print("Provide OpenWeatherMap APIKEY with -k or --apikey")
        exit(-1)
    if args.location is None:
        print("Provide OpenWeatherMap location with -l or --location")
        exit(-1)
    weather["location_key"] = args.location
    weather["api_key"] = args.apikey
    weather["classic-icons"] = args.classic_icons
    weather["metric_units"] = args.metric
    weather["debug"] = args.debug

    weather["icon_directory"] = os.getenv("HOME") + "/.local/share/weather/icons"
    icon_dir = Path(weather["icon_directory"])
    if not icon_dir.exists():
        icon_dir.mkdir(parents=True, exist_ok=True)

    weather["data_directory"] = os.getenv("HOME") + "/.local/share/weather/data"
    data_dir = Path(weather["data_directory"])
    if not data_dir.exists():
        data_dir.mkdir(parents=True, exist_ok=True)

    get_weather_info()

if __name__ == "__main__":
    main()
