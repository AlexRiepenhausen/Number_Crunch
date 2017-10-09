'''
convert a string into an array of ascii numbers'''
def string_to_ascii_arr(string):
  
    ascii = []
    for char in string:
        ascii.append(ord(char))
      
    return ascii

