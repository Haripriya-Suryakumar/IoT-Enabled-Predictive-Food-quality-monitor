Predictive Food Quality Monitoring using IoT and ML

This project focuses on predicting the freshness and remaining shelf life of food products such as meat, dairy, and fruits using IoT-based sensing and machine learning. Real-time data is collected using an ESP32 microcontroller connected to gas sensors (MQ-135, MQ-3, MQ-137) and a DHT22 temperature-humidity sensor. The readings are processed by trained machine learning models deployed on Azure Cloud to classify the food’s freshness stage and estimate remaining shelf life. A local Flask-based interface and ThingSpeak dashboard are used for live monitoring and visualization.

Data Collection

The datasets were collected under stable room temperature conditions. Each dataset represents one category of food, meat, dairy, or fruit and contains sensor readings captured at fixed intervals over multiple hours.
The collected data includes temperature, humidity, and gas concentration values (in ppm) corresponding to different freshness stages: fresh, slightly spoiled, and spoiled.
These readings were used to label and train separate machine learning models for each category.
The MQ series sensors were calibrated for over 24 hours. 

Hardware Used

*ESP32 Microcontroller
*MQ-135 Gas Sensor
*MQ-3 Gas Sensor
*MQ-137 Gas Sensor
*DHT22 Temperature & Humidity Sensor

Software Used

*Arduino IDE (for firmware)
*Python (scikit-learn, joblib, pickle)
*Flask Framework
*ThingSpeak Cloud Platform
*Azure Web App for model deployment