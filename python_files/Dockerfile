FROM python:3
ADD tibber_prices.py /
ADD auth_secrets.py /
RUN pip install requests
RUN pip install pandas
RUN pip install matplotlib
CMD [ "python", "./tibber_prices.py" ]
