# Include needed libraries

import requests
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
from datetime import datetime, timedelta
import time
import pytz
from auth_secrets import tibber_token

timezone = pytz.timezone("Europe/Stockholm")

prev_max_y = 2
boot_count = 0

def get_tibber_graph():

    global prev_max_y
    global boot_count

    headers = {"Authorization": "Bearer " + tibber_token}

    # Set GraphQL query
    query = """{
      viewer {
        homes {
          currentSubscription{
            priceInfo{
              current{
                total
                startsAt
              }
              today {
                total
                startsAt
              }
              tomorrow {
                total
                startsAt
              }
            }
          }
        }
      }
    }"""

    # Call the API
    endpoint = "https://api.tibber.com/v1-beta/gql"
    result = requests.post(endpoint, json={"query": query}, headers=headers).json()

    # Break up response into usable pieces
    json_current = result["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["current"]
    json_today = result["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["today"]
    json_tomorrow = result["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"]["tomorrow"]

    # Import responses to Pandas
    result_current = pd.json_normalize(json_current)
    result_today = pd.json_normalize(json_today)
    result_tomorrow = pd.json_normalize(json_tomorrow)

    # Extract hours from startsAt in all dfs
    all_dfs = [result_current, result_today, result_tomorrow]

    for df in all_dfs:
        if not df.empty:
            df["startsAt"] = pd.to_datetime(df["startsAt"])
            df["hour"] = df["startsAt"].dt.hour

    ### Create graph
    plt.figure(figsize=(8, 6))
    ax = result_today.plot(
        x="hour",
        y="total",
        color="black",
        linestyle="solid",
        kind="line",
        label="today",
    )

    result_current.plot(
        ax=ax, x="hour", y="total", s=64, color="black", marker="o", kind="scatter"
    )

    if not result_tomorrow.empty:
        result_tomorrow.plot(
            ax=ax,
            x="hour",
            y="total",
            color="black",
            linestyle="dashed",
            kind="line",
            label="tomorrow",
        )

    # Round current price for title
    current_price_rounded_string = str(np.around(result_current['total'][0], 2))

    # Set graph options
    plt.xlabel("Hour")
    plt.ylabel("Kr / kWh")
    plt.title("Elpris (Now: " + current_price_rounded_string + "Kr)")
    plt.legend()
    plt.xticks(range(24))
    plt.grid(visible=True, which="major", axis="x", color="#a3a3a3") # Lighter: #c2c4c3
    plt.annotate(
        "Last updated on " + datetime.now(tz=timezone).strftime("%Y%m%d %H:%M:%S"),
        (0, 0),
        (0, -20),
        xycoords="axes fraction",
        textcoords="offset points",
        va="top",
        size=8
    )
    plt.style.use("grayscale")

    # Round y axis values
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.1f'))

    # Set axis limits
    if result_tomorrow.empty:
        if boot_count == 1:
            print("First Boot...")
            max_y = max(result_today["total"].max(), 2)
        else:
            print("Boot_count: " + str(boot_count))
            print("Prev_max_y: " + str(prev_max_y))
            max_y = prev_max_y
    else:
        max_y = max(result_today["total"].max(), result_tomorrow["total"].max())

    prev_max_y = max_y
    y_lim = max(max_y * 1.1, 2)

    plt.ylim(0, y_lim)
    plt.xlim(0, 23)

    # Make layout tight
    plt.tight_layout(pad=1, w_pad=1, h_pad=1)

    ### Resize to correct aspect ratio
    # Define y-unit to x-unit ratio
    ratio = 0.75

    # Get x and y limits
    x_left, x_right = ax.get_xlim()
    y_low, y_high = ax.get_ylim()

    #set aspect ratio
    ax.set_aspect(abs((x_right - x_left) / (y_low - y_high)) * ratio)

    # Save graph (Increase in dpi causes both x and y-axis to increase)
    plt.savefig("./shared/elpris.png", dpi=125)


while True:

    boot_count += 1

    print("Getting graph:")
    get_tibber_graph()
    print("Waiting...")

    if(datetime.now(tz=timezone).hour == 13 and datetime.now(tz=timezone).minute < 15):
        refresh_rate_mins = 15
    else:
        refresh_rate_mins = 60

    current_minute = datetime.now(tz=timezone).minute
    minutes_wait = refresh_rate_mins - (current_minute % refresh_rate_mins)

    alarm_minute = 0 if current_minute + minutes_wait == 60 else current_minute + minutes_wait

    dt = datetime.now(tz=timezone) + timedelta(minutes=minutes_wait)
    dt = dt.replace(minute=alarm_minute).replace(second=0)

    while datetime.now(tz=timezone) < dt:
        print("Next run at " + dt.strftime("%Y%m%d %H:%M:%S"))
        print("Now: " + datetime.now(tz=timezone).strftime("%Y%m%d %H:%M:%S"))
        time.sleep(30)
