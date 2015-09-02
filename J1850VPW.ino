/*****************************************************************************
J1850 VPW Sketch
For Arduino to Car Communications via OBD-II
Written by Connor Murphy
http://connorwmurphy.com/

Released under GNU/GPL v3:

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

const int PIN_OUT = 8; 
const int PIN_IN = 9;

String msg = "";
boolean isValid = false;

void setup()
{
    Serial.begin(9600);
    Serial.println("Started");

    pinMode(PIN_OUT, OUTPUT);
    pinMode(PIN_IN, INPUT);
}

/*
CRC8 Function Written by Leonardo Miliani
http://www.leonardomiliani.com/en/2013/un-semplice-crc8-per-arduino/

Original Commentation:
CRC-8 - based on the CRC8 formulas by Dallas/Maxim
Code released under the terms of the GNU GPL 3.0 license
*/
byte CRC8(const byte *data, byte len) 
{
    byte crc = 0x00;
    
    while (len--) 
    {
        byte extract = *data++;
        
        for (byte tempI = 8; tempI; tempI--) 
        {
            byte sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            
            if (sum) 
            {
                crc ^= 0x8C;
            }
            
            extract >>= 1;
        }
    }
    
    return crc;
}

void parseResponse(String before, int after[])
{
    Serial.println("Message byteCount: "+String(before.length())+"/8 = "+String(before.length()/8));
    
    for(int byteCount = 0; byteCount < before.length()/8; byteCount++)
    {
        int currentByte = 0;
        
        for(int bitCount = 0; bitCount < 8; bitCount++)
        {
            // Reason for 8 - (bitCount + 1):
            // bitWrite writes with 0 at the rightmost bit whereas
            // we are reading from the leftmost bit 
            
            // Then we have to take the byte and bit count and get the position
            // of the 1 or 0 in the original string and convert it to an integer
            bitWrite(currentByte, 8 - (bitCount + 1), String(before.charAt((byteCount*8) + bitCount)).toInt());
        } 
        
        after[byteCount] = currentByte;
    }
}

void loop()
{
    while(Serial.available() > 0)
    {
        msg += (char)Serial.read();
    }
    
    if(msg == "itemp")
    {
        sendPacket(0x01, 0x0F);
        isValid = true;
    } 
    else if(msg == "speed")
    {
        sendPacket(0x01, 0x0D);
        isValid = true;
    }
    
    // This selection will trigger if the request was valid and we
    // are expecting a response from the vehicle. 
    if(isValid)
    {
        digitalWrite(PIN_OUT, LOW);
        
        boolean reachedEOF = false;
        boolean receivedResponse = false;
        long responseStart = 0L;
        unsigned long startMicros = 0;
        int expectedBitVoltage = -1; // It is -1 because after the SOD bit (200us high)
                                     // we will be expecting a low bit
        int currentBitVoltage = 0;
        String response = "";
        
        while(!receivedResponse)
        {
            if(digitalRead(PIN_IN) == HIGH && responseStart == 0L)
            {
                responseStart = millis();
            }
            if(digitalRead(PIN_IN) == LOW && responseStart > 0L)
            {
                long difference = abs(millis()-responseStart);
            
                if(difference >= 195)
                {
                    startMicros = micros(); 
                    receivedResponse = true;
                    Serial.println("Received response");   
                }
            }     
        }
        
        while(!reachedEOF)
        {
            currentBitVoltage = digitalRead(PIN_IN);
            
            if(currentBitVoltage != expectedBitVoltage)
            {
                // Changed, we are now on a different bit
                unsigned long endMicros = micros();
                long difference = (endMicros - startMicros);

                startMicros = endMicros;
                expectedBitVoltage = -expectedBitVoltage;
                
                if(difference < 0)
                {
                    difference = endMicros - (4294967295 - startMicros);
                }
                
                // Check to see what the difference between comparing the duration of the bit was 
                // to the expected duration of a long or short bit
                if(abs(128 - difference) < abs(64 - difference) && abs(128 - difference) < abs(200 - difference))
                {
                    //Serial.print("Long bit length: ");
                    //Serial.println(difference);

                    // Long bit    
                    if(expectedBitVoltage > 0)
                    {
                        response += "0";    
                        //Serial.println("0");
                    }
                    else
                    {
                        response += "1";    
                        //Serial.println("1");
                    }
                }
                else if(abs(64 - difference) < abs(128 - difference) && abs(64 - difference) < abs(200 - difference))
                {
                    //Serial.print("Short bit length: ");
                    //Serial.println(difference);

                     // Short bit 
                    if(expectedBitVoltage > 0)
                    {
                        response += "1";    
                        //Serial.println("1");
                    }
                    else
                    {
                        response += "0"; 
                        //Serial.println("0");
                    }
                }
                else if(abs(200 - difference) < abs(128 - difference) && abs(200 - difference) < abs(64 - difference))
                {
                    // End of Frame Detected
                    //Serial.print("EOF Length: ");
                    //Serial.println(difference);
                    reachedEOF = true;
                }
            }
        }
        
        Serial.println("Response: ");
        int responseBytes[response.length()/8];
        parseResponse(response, responseBytes);
        
        for(int i_byte = 0; i_byte < (sizeof(responseBytes) / sizeof(int)); i_byte++)
        {
            Serial.print("0x"+String(responseBytes[i_byte], HEX)+" ");
        }    
        
        Serial.println();
        
        isValid = false;
        msg = "";
    }
}

// Packet:
//     |----------------Header-----------------|
// SOD Priority/Type TargetAddress SourceAddress Mode PID CRC EOD
int sendPacket(int mode, int pid)
{                           // #76543210
    int priority = 0x68;    //0b01101000
    //int priority = 0b00001100;
    int targetAddr = 0x6A; // 0x10 = PCM
                           // 0xF1 = Off-Board Tool 
    int sourceAddr = 0xF1; // [EDIT SCRATCH THIS]: Anything between 0xF0 to 0xFF works just 
    // just to say its a scan tool
    int packet[8*6]; // 8 bits per byte X 6 bytes
    byte packetBytes[6]; // The 6 bytes for CRC building
    byte crc = 0x00;
    
    // Build CRC ahead of time
    packetBytes[0] = priority;
    packetBytes[1] = targetAddr;
    packetBytes[2] = sourceAddr;
    packetBytes[3] = 0x01;
    packetBytes[4] = pid;
    
    crc = CRC8(packetBytes, 5);
    packetBytes[5] = crc;

    // Assemble packet from known request parameters
    // NOTE: 8 is the most significant (leading) bit
    for(int i = 8; i > 0; i--) 
    {
        packet[0  + (8-i)] = getBit(priority, i);
        packet[8  + (8-i)] = getBit(targetAddr, i);
        packet[16 + (8-i)] = getBit(sourceAddr, i);
        packet[24 + (8-i)] = getBit(mode, i);
        packet[32 + (8-i)] = getBit(pid, i);
        packet[40 + (8-i)] = getBit(crc, i);
    }
    
    Serial.println("Packet:");
    for(int i = 0; i < 6; i++)
    {
        Serial.print(String(packetBytes[i], HEX)+" ");
        String str_currentByte = "";
        byte by_currentByte = 0x00;
        
        for(int i_bit = 0; i_bit < 8; i_bit++)
        {
            str_currentByte = str_currentByte + String(packet[i + i_bit]);
        }
        
        
    }
    Serial.println();
    
    sendSOD();
    
    int expectedVoltage = -1;
    for(int i = 0; i < (sizeof(packet) / sizeof(int)); i++)
    {
        sendBit(packet[i], expectedVoltage);
        expectedVoltage = -expectedVoltage;
    }

    //sendEOD();    
}

void sendSOD()
{
    digitalWrite(PIN_OUT, HIGH);
    delayMicroseconds(200);   
    digitalWrite(PIN_OUT, LOW); 
}

void sendEOF()
{
    digitalWrite(PIN_OUT, HIGH);
    delayMicroseconds(280);
    digitalWrite(PIN_OUT, LOW);
}

void sendEOD()
{
    digitalWrite(PIN_OUT, LOW);
    delayMicroseconds(200);  
    digitalWrite(PIN_OUT, LOW);  
}

void sendBit(int value, int expectedVoltage)
{
    if(expectedVoltage == 1)
    {
        digitalWrite(PIN_OUT, HIGH);
        
        if(value == 1)
        {
            delayMicroseconds(64);
        } else {
            delayMicroseconds(128);
        }
    } else {
        digitalWrite(PIN_OUT, LOW);
        
        if(value == 1)
        {
            delayMicroseconds(128);
        } else {
            delayMicroseconds(64);
        }
    }
}

// NOTE: 8 is the most significant (leading) bit
int getBit(int number, int n)
{
    int mask = 1;
    mask = mask << (n-1);

    return (mask&number) >> (n-1);    
}

