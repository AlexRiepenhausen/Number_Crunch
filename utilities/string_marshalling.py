'''Convert binary number to decimal'''
def anyBinaryToDecimal(binary):
    result = 0
    counter = 1
    for i in binary[:-1]: # Iterating through input in reverse order
        result += int(i) * counter
        counter =  counter*2

    return result

'''CONSTRUCTION ->'''
'''Converts a 32bit integer to a bit array of size 8 - value supplied must be ascii representation, else space is returned (32)'''
def _32int_to_8bitarray(number):
    if number > 255:
        return [0,0,1,0,0,0,0,0] #space
    elif number < 0:
        return [0,0,1,0,0,0,0,0] #space 
    else:
        bit_arr = [int(digit) for digit in bin(number)[2:]] 
      
        #size of result has to be 8
        if len(bit_arr) < 8:
            
            size = len(bit_arr)
            result = [0,0,0,0,0,0,0,0]
            
            bit_count = 0
            for index in range(0,8):
                if (8-index)-size <= 0:
                    result[index] = bit_arr[bit_count]
                    bit_count = bit_count + 1
                    
            return result
        
        else:
            
            return bit_arr
'''-----------------------------------------------------------------------------------------'''          
          
'''
takes 4 arrays of size 8 representing a byte and constructs an array of size 32, 
effectively concatenating those 4 arrays'''
def _four_8bitarrays_to_32intarray(bit_arr):
    
    _32_bit_arr = [0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0]
    
    for index1 in range(4,0,-1):
        start = (index1-0)*8
        end   = (index1-1)*8 
        bit_count = 7
        for index2 in range(start,end,-1):
            _32_bit_arr[index2-1] = bit_arr[index1-1][bit_count]
            bit_count = bit_count - 1
    
    return _32_bit_arr

'''-----------------------------------------------------------------------------------------'''   

def _32intarray_to_int(_32_int_arr):

    if len(_32_int_arr) != 32:
        for i in range(0,32 - len(_32_int_arr)):
            _32_int_arr.append(0)

    i = 0
    for bit in _32_int_arr:
        i = (i << 1) | bit
    return i

'''-----------------------------------------------------------------------------------------'''   

'''REVERSE CONSTRUCTION <-'''
'''Converts a 32bit integer to a bit array of size 32'''
def _32int_to_32bitarray(number): 
    bit_arr = [int(digit) for digit in bin(number)[2:]]

    #size of result has to be 8
    if len(bit_arr) < 32:
            
        size = len(bit_arr)
        result = [0,0,0,0,0,0,0,0,
                  0,0,0,0,0,0,0,0,
                  0,0,0,0,0,0,0,0,
                  0,0,0,0,0,0,0,0]
            
        bit_count = 0
        for index in range(0,32):
            if (32-index)-size <= 0:
                result[index] = bit_arr[bit_count]
                bit_count = bit_count + 1
                    
        return result
        
    else:
            
        return bit_arr       
              
'''-----------------------------------------------------------------------------------------'''    

'''converts bit array of size 32 to 4 characters'''
def _32intarray_to_four_8bitchars(_32_int_arr):
    
    char_arr = []
    
    for index1 in range (0,4):
        start = index1*8
        end   = index1*8 + 8
        
        _8_int_arr = []      
        for index2 in range(start,end):
            _8_int_arr.append(_32_int_arr[index2])
            
    
        i = 0
        for bit in _8_int_arr:
            i = (i << 1) | bit
        
        char_arr.append(chr(i))
        

    return char_arr

'''-----------------------------------------------------------------------------------------'''   
'''Converts a piece of string into an array of integers. Each 32 bit integer stores 4 8-bit chars'''
def convert_string_to_integer_parcel(string, bytes_per_string):
    
    '''size is the number of bytes that every string gets'''
    
    ascii     = []
    count     = 0
    
    #produce array of ascii characters
    for char in string:    
        ascii.append(ord(char))
        count = count + 1
       
    while count < bytes_per_string:
        ascii.append(32) #32 is the ascii for space
        count = count + 1
    
    #convert each element of the ascii array to a bit array of size 8
    ascii_byte_arr = []
    for index1 in range(0, bytes_per_string):
        ascii_byte_arr.append(_32int_to_8bitarray(ascii[index1]))
    
    #now take 4 8-bit arrays each and construct an integer
    #the integer is then put into an array of size bytes_per_string/4
    #This saves 75% of memory space
    parcel    = []
    for index2 in range(0, bytes_per_string/4):
        
        start = index2*4
        
        _4_8bit_arrays = []
        _4_8bit_arrays.append(ascii_byte_arr[start+0])
        _4_8bit_arrays.append(ascii_byte_arr[start+1])
        _4_8bit_arrays.append(ascii_byte_arr[start+2])
        _4_8bit_arrays.append(ascii_byte_arr[start+3]) 
     
        _32bit_array = _four_8bitarrays_to_32intarray(_4_8bit_arrays)
        parcel.append(_32intarray_to_int(_32bit_array))
    
    return parcel

'''-----------------------------------------------------------------------------------------'''   
'''Undoes convert_string_to_integer_parcel(string, bytes_per_string)'''
def convert_integer_parcel_to_string(integer_array, num_chars):
    
    array_of_32bit_arrays = []
    
    #convert integers into their bit representation
    for i in integer_array:
        array_of_32bit_arrays.append(_32int_to_32bitarray(i))

    original_string = ''
    for index1 in range(0,num_chars/4):   
        _4char_parcel = _32intarray_to_four_8bitchars(array_of_32bit_arrays[index1])
        string = ''.join(_4char_parcel)
        original_string = original_string + string

    return original_string
