FROM python:3.11-slim

RUN apt-get update && \
    apt-get install -y python3-tk xvfb && \
    pip install --no-cache-dir tk

COPY main.py /app/main.py
WORKDIR /app

CMD ["xvfb-run", "python", "main.py"]
