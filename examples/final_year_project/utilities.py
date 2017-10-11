'''
convert a string into an array of ascii numbers
Default size of string is 
'''
def string_to_ascii_arr(string, size):
  
    '''parameter 1: string as in the string data'''
    '''parameter 2: size in bytes''' 
    
    '''
    if the string is smaller than the byte size, 
    0 will be placed after the final character until size is reached
    '''
    
    ascii = []
    count = 0
    for char in string:
        ascii.append(ord(char))
        count = count + 1
    
    while count < size:
        ascii.append(0)
        count = count + 1
      
    return ascii

