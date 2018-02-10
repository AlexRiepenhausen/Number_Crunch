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
    
    def write_to_csv(self, new_filename, array):
             
        with open(new_filename, "wb") as f:
            writer = csv.writer(f)
            
            writer.writerow(["Unique IDs"])
            for row in range (0, len(array)):
                writer.writerow([array[row]])