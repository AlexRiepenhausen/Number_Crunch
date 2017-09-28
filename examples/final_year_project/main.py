
"""
Main file for executing the application
"""

from parser import parser

getData = parser('../../resources/test.csv')
raw_data = getData.read_data()

for entry in raw_data:
    print entry