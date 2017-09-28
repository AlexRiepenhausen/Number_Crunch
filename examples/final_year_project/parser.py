"""
This section is used for parsing the data from the csv file provided
"""

import csv

class parser(object):
    
    def __init__(self, filename):
        self.filename = filename

    def read_data(self):

        with open(self.filename, 'rb') as csvfile:
    
            datareader = csv.reader(csvfile, delimiter=',', quotechar='|')
    
            #define 2D-array to hold all entries of data points
            data_parcel = []
    
            for row in datareader:
                data_parcel.append(row)
                
            #return the data
            return data_parcel
