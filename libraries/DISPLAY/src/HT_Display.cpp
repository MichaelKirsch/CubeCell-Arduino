#include <HT_Display.h>

ScreenDisplay::ScreenDisplay() {

	displayWidth = 128;
	displayHeight = 64;
	displayBufferSize = displayWidth * displayHeight / 8;
	color = WHITE;
	geometry = GEOMETRY_128_64;
	textAlignment = TEXT_ALIGN_LEFT;
	fontData = ArialMT_Plain_10;
	fontTableLookupFunction = DefaultFontTableLookup;
	buffer = NULL;
#ifdef DISPLAY_DOUBLE_BUFFER
	buffer_back = NULL;
#endif
}

ScreenDisplay::~ScreenDisplay() {
  end();
}

bool ScreenDisplay::init() {

	if(this->rst!=-1)
	{
		pinMode(this->rst,OUTPUT);
		digitalWrite(this->rst,HIGH);
		delay(1);
		digitalWrite(this->rst,LOW);
		delay(1);
		digitalWrite(this->rst,HIGH);
		delay(1);
	}

	logBufferSize = 0;
	logBufferFilled = 0;
	logBufferLine = 0;
	logBufferMaxLines = 0;
    logBuffer = NULL;
  if (!this->connect()) {
    DEBUG_DISPLAY("[DISPLAY][init] Can't establish connection to display\n");
    return false;
  }

  if(this->buffer==NULL&& displayType==OLED) {
  	this->buffer = (uint8_t*) malloc((sizeof(uint8_t) * displayBufferSize) + getBufferOffset());
	this->buffer += getBufferOffset();
  if(!this->buffer) {
    DEBUG_DISPLAY("[DISPLAY][init] Not enough memory to create display\n");
    return false;
  }
  }

  #ifdef DISPLAY_DOUBLE_BUFFER
  if(this->buffer_back==NULL && displayType==OLED) {
  this->buffer_back = (uint8_t*) malloc((sizeof(uint8_t) * displayBufferSize) + getBufferOffset());
  this->buffer_back += getBufferOffset();

  if(!this->buffer_back) {
    DEBUG_DISPLAY("[DISPLAY][init] Not enough memory to create back buffer\n");
    free(this->buffer - getBufferOffset());
    return false;
  }
  }
  #endif


  sendInitCommands();
  if(displayType==OLED)
      resetDisplay();

  return true;
}

void ScreenDisplay::end() {
  if (this->buffer && displayType==OLED) { free(this->buffer - getBufferOffset()); this->buffer = NULL; }
  #ifdef DISPLAY_DOUBLE_BUFFER
  if (this->buffer_back && displayType==OLED) { free(this->buffer_back - getBufferOffset()); this->buffer_back = NULL; }
  #endif
  if (this->logBuffer != NULL) { free(this->logBuffer); this->logBuffer = NULL; }
  if(this->rst!=-1)
  {
    pinMode(this->rst,ANALOG);
  }
}

void ScreenDisplay::resetDisplay(void) {
  clear();
  #ifdef DISPLAY_DOUBLE_BUFFER
  if(displayType==OLED)
      memset(buffer_back, 1, displayBufferSize);
  #endif
  display();
}

void ScreenDisplay::setColor(DISPLAY_COLOR color) {
  this->color = color;
}

DISPLAY_COLOR ScreenDisplay::getColor() {
  return this->color;
}

void ScreenDisplay::setPixel(int16_t x, int16_t y) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case WHITE:   buffer[x + (y >> 3) * this->width()] |=  (1 << (y & 7)); break;
      case BLACK:   buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y >> 3) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}

void ScreenDisplay::setPixelColor(int16_t x, int16_t y, DISPLAY_COLOR color) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case WHITE:   buffer[x + (y >> 3) * this->width()] |=  (1 << (y & 7)); break;
      case BLACK:   buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y >> 3) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}

void ScreenDisplay::clearPixel(int16_t x, int16_t y) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case BLACK:   buffer[x + (y >> 3) * this->width()] |=  (1 << (y & 7)); break;
      case WHITE:   buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y >> 3) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}


// Bresenham's algorithm - thx wikipedia and Adafruit_GFX
void ScreenDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx >> 1;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {  
      setPixel(y0, x0);
    } else {
      setPixel(x0, y0);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void ScreenDisplay::drawRect(int16_t x, int16_t y, int16_t width, int16_t height) {
  drawHorizontalLine(x, y, width);
  drawVerticalLine(x, y, height);
  drawVerticalLine(x + width - 1, y, height);
  drawHorizontalLine(x, y + height - 1, width);
}

void ScreenDisplay::fillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height) {
  for (int16_t x = xMove; x < xMove + width; x++) {
    drawVerticalLine(x, yMove, height);
  }
}

void ScreenDisplay::drawCircle(int16_t x0, int16_t y0, int16_t radius) {
  int16_t x = 0, y = radius;
	int16_t dp = 1 - radius;
	do {
		if (dp < 0)
			dp = dp + ((++x) << 1) + 3;
		else
			dp = dp + ((++x) << 1) - ((--y) << 1) + 5;

		setPixel(x0 + x, y0 + y);     //For the 8 octants
		setPixel(x0 - x, y0 + y);
		setPixel(x0 + x, y0 - y);
		setPixel(x0 - x, y0 - y);
		setPixel(x0 + y, y0 + x);
		setPixel(x0 - y, y0 + x);
		setPixel(x0 + y, y0 - x);
		setPixel(x0 - y, y0 - x);

	} while (x < y);

  setPixel(x0 + radius, y0);
  setPixel(x0, y0 + radius);
  setPixel(x0 - radius, y0);
  setPixel(x0, y0 - radius);
}

void ScreenDisplay::drawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads) {
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  while (x < y) {
    if (dp < 0)
      dp = dp + ((++x) << 1) + 3;
    else
      dp = dp + ((++x) << 1) - ((--y) << 1) + 5;
    if (quads & 0x1) {
      setPixel(x0 + x, y0 - y);
      setPixel(x0 + y, y0 - x);
    }
    if (quads & 0x2) {
      setPixel(x0 - y, y0 - x);
      setPixel(x0 - x, y0 - y);
    }
    if (quads & 0x4) {
      setPixel(x0 - y, y0 + x);
      setPixel(x0 - x, y0 + y);
    }
    if (quads & 0x8) {
      setPixel(x0 + x, y0 + y);
      setPixel(x0 + y, y0 + x);
    }
  }
  if (quads & 0x1 && quads & 0x8) {
    setPixel(x0 + radius, y0);
  }
  if (quads & 0x4 && quads & 0x8) {
    setPixel(x0, y0 + radius);
  }
  if (quads & 0x2 && quads & 0x4) {
    setPixel(x0 - radius, y0);
  }
  if (quads & 0x1 && quads & 0x2) {
    setPixel(x0, y0 - radius);
  }
}


void ScreenDisplay::fillCircle(int16_t x0, int16_t y0, int16_t radius) {
  int16_t x = 0, y = radius;
	int16_t dp = 1 - radius;
	do {
		if (dp < 0)
      dp = dp + ((++x) << 1) + 3;
    else
      dp = dp + ((++x) << 1) - ((--y) << 1) + 5;

    drawHorizontalLine(x0 - x, y0 - y, 2*x);
    drawHorizontalLine(x0 - x, y0 + y, 2*x);
    drawHorizontalLine(x0 - y, y0 - x, 2*y);
    drawHorizontalLine(x0 - y, y0 + x, 2*y);


	} while (x < y);
  drawHorizontalLine(x0 - radius, y0, radius << 1);

}

void ScreenDisplay::drawHorizontalLine(int16_t x, int16_t y, int16_t length) {

  if (y < 0 || y >= this->height()) { return; }
  
   if (x < 0) {
	 length += x;
	 x = 0;
   }
  
   if ( (x + length) > this->width()) {
	 length = (this->width() - x);
   }
  
   if (length <= 0) { return; }
  
   uint8_t * bufferPtr = buffer;
   bufferPtr += (y >> 3) * this->width();
   bufferPtr += x;
  
   uint8_t drawBit = 1 << (y & 7);
  
   switch (color) {
	 case WHITE:   while (length--) {
		 *bufferPtr++ |= drawBit;
	   }; break;
	 case BLACK:   drawBit = ~drawBit;	 while (length--) {
		 *bufferPtr++ &= drawBit;
	   }; break;
	 case INVERSE: while (length--) {
		 *bufferPtr++ ^= drawBit;
	   }; break;
   }

}

void ScreenDisplay::drawVerticalLine(int16_t x, int16_t y, int16_t length) {
  if (x < 0 || x >= this->width()) return;

  if (y < 0) {
    length += y;
    y = 0;
  }

  if ( (y + length) > this->height()) {
    length = (this->height() - y);
  }

  if (length <= 0) return;


  uint8_t yOffset = y & 7;
  uint8_t drawBit;
  uint8_t *bufferPtr = buffer;

  bufferPtr += (y >> 3) * this->width();
  bufferPtr += x;

  if (yOffset) {
    yOffset = 8 - yOffset;
    drawBit = ~(0xFF >> (yOffset));

    if (length < yOffset) {
      drawBit &= (0xFF >> (yOffset - length));
    }

    switch (color) {
      case WHITE:   *bufferPtr |=  drawBit; break;
      case BLACK:   *bufferPtr &= ~drawBit; break;
      case INVERSE: *bufferPtr ^=  drawBit; break;
    }

    if (length < yOffset) return;

    length -= yOffset;
    bufferPtr += this->width();
  }

  if (length >= 8) {
    switch (color) {
      case WHITE:
      case BLACK:
        drawBit = (color == WHITE) ? 0xFF : 0x00;
        do {
          *bufferPtr = drawBit;
          bufferPtr += this->width();
          length -= 8;
        } while (length >= 8);
        break;
      case INVERSE:
        do {
          *bufferPtr = ~(*bufferPtr);
          bufferPtr += this->width();
          length -= 8;
        } while (length >= 8);
        break;
    }
  }

  if (length > 0) {
    drawBit = (1 << (length & 7)) - 1;
    switch (color) {
      case WHITE:   *bufferPtr |=  drawBit; break;
      case BLACK:   *bufferPtr &= ~drawBit; break;
      case INVERSE: *bufferPtr ^=  drawBit; break;
    }
  }
}

void ScreenDisplay::drawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
  uint16_t radius = height >> 1;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = radius << 1;
  uint16_t innerRadius = radius - 2;

  setColor(WHITE);
  drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
  drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

  uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 100;

  fillCircle(xRadius, yRadius, innerRadius);
  fillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
  fillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
}

void ScreenDisplay::drawFastImage(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *image) {
  drawInternal(xMove, yMove, width, height, image, 0, 0);
}

void ScreenDisplay::drawXbm(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *xbm) {
  int16_t widthInXbm = (width + 7) >> 3;
  uint8_t data = 0;

  for(int16_t y = 0; y < height; y++) {
    for(int16_t x = 0; x < width; x++ ) {
      if (x & 7) {
        data >>= 1; // Move a bit
      } else {  // Read new data every 8 bit
        data = pgm_read_byte(xbm + (x >> 3) + y * widthInXbm);
      }
      // if there is a bit draw it
      if (data & 0x01) {
        setPixel(xMove + x, yMove + y);
      }
    }
  }
}

void ScreenDisplay::drawXbmRotateDegCenter(int16_t x, int16_t y, int16_t width, int16_t height, float angleDeg, const uint8_t *xbm) {
    int16_t widthInXbm = (width + 7) >> 3;
    uint8_t data = 0;
    uint16_t offsetX = width/2;
    uint16_t offsetY = height/2;

    float degrees = (angleDeg + 90) * 0.017453; //convert to radians
    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            if (x & 7) {
                data >>= 1; // Move a bit
            } else {  // Read new data every 8 bit
                data = pgm_read_byte(xbm + (x >> 3) + y * widthInXbm);
            }
            // if there is a bit draw it
            if (data & 0x01) {
                int16_t new_x = (x - offsetX) * cos(degrees) - (y - offsetY) * sin(degrees);
                int16_t new_y = (x - offsetX) * sin(degrees) + (y - offsetY) * cos(degrees);
                setPixel(x + new_x, y + new_y);
            }
        }
    }
}

void ScreenDisplay::drawXbmRotateRadCenter(int16_t x, int16_t y, int16_t width, int16_t height, float angleRad, const uint8_t *xbm) {
    int16_t widthInXbm = (width + 7) >> 3;
    uint8_t data = 0;
    uint16_t offsetX = width/2;
    uint16_t offsetY = height/2;

    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            if (x & 7) {
                data >>= 1; // Move a bit
            } else {  // Read new data every 8 bit
                data = pgm_read_byte(xbm + (x >> 3) + y * widthInXbm);
            }
            // if there is a bit draw it
            if (data & 0x01) {
                int16_t new_x = (x - offsetX) * cos(angleRad) - (y - offsetY) * sin(angleRad);
                int16_t new_y = (x - offsetX) * sin(angleRad) + (y - offsetY) * cos(angleRad);
                setPixel(x + new_x, y + new_y);
            }
        }
    }
}

void ScreenDisplay::drawXbmRotateRadOffset(int16_t x, int16_t y, int16_t width, int16_t height, float angleRad, int16_t offsetX, int16_t offsetY, const uint8_t *xbm) {
    int16_t widthInXbm = (width + 7) >> 3;
    uint8_t data = 0;

    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            if (x & 7) {
                data >>= 1; // Move a bit
            } else {  // Read new data every 8 bit
                data = pgm_read_byte(xbm + (x >> 3) + y * widthInXbm);
            }
            // if there is a bit draw it
            if (data & 0x01) {
                int16_t new_x = (x - offsetX) * cos(angleRad) - (y - offsetY) * sin(angleRad);
                int16_t new_y = (x - offsetX) * sin(angleRad) + (y - offsetY) * cos(angleRad);
                setPixel(x + new_x, y + new_y);
            }
        }
    }
}

void ScreenDisplay::drawXbmRotateDegOffset(int16_t x, int16_t y, int16_t width, int16_t height, float angleDeg, int16_t offsetX, int16_t offsetY, const uint8_t *xbm) {
    int16_t widthInXbm = (width + 7) >> 3;
    uint8_t data = 0;
    float degrees = (angleDeg + 90) * 0.017453; //convert to radians
    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            if (x & 7) {
                data >>= 1; // Move a bit
            } else {  // Read new data every 8 bit
                data = pgm_read_byte(xbm + (x >> 3) + y * widthInXbm);
            }
            // if there is a bit draw it
            if (data & 0x01) {
                int16_t new_x = (x - offsetX) * cos(degrees) - (y - offsetY) * sin(degrees);
                int16_t new_y = (x - offsetX) * sin(degrees) + (y - offsetY) * cos(degrees);
                setPixel(x + new_x, y + new_y);
            }
        }
    }
}

void ScreenDisplay::drawIco16x16(int16_t xMove, int16_t yMove, const char *ico, bool inverse) {
  uint16_t data;

  for(int16_t y = 0; y < 16; y++) {
    data = pgm_read_byte(ico + (y << 1)) + (pgm_read_byte(ico + (y << 1) + 1) << 8);
    for(int16_t x = 0; x < 16; x++ ) {
      if ((data & 0x01) ^ inverse) {
        setPixelColor(xMove + x, yMove + y, WHITE);
      } else {
        setPixelColor(xMove + x, yMove + y, BLACK);
      }
      data >>= 1; // Move a bit
    }
  }
}

void ScreenDisplay::drawStringInternal(int16_t xMove, int16_t yMove, char* text, uint16_t textLength, uint16_t textWidth) {
  uint8_t textHeight       = pgm_read_byte(fontData + HEIGHT_POS);
  uint8_t firstChar        = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint16_t sizeOfJumpTable = pgm_read_byte(fontData + CHAR_NUM_POS)  * JUMPTABLE_BYTES;

  uint16_t cursorX         = 0;
  uint16_t cursorY         = 0;

  switch (textAlignment) {
    case TEXT_ALIGN_CENTER_BOTH:
      yMove -= textHeight >> 1;
    // Fallthrough
    case TEXT_ALIGN_CENTER:
      xMove -= textWidth >> 1; // divide by 2
      break;
    case TEXT_ALIGN_RIGHT:
      xMove -= textWidth;
      break;
    case TEXT_ALIGN_LEFT:
      break;
  }

  // Don't draw anything if it is not on the screen.
  if (xMove + textWidth  < 0 || xMove > this->width() ) {return;}
  if (yMove + textHeight < 0 || yMove > this->height() ) {return;}

  for (uint16_t j = 0; j < textLength; j++) {
    int16_t xPos = xMove + cursorX;
    int16_t yPos = yMove + cursorY;

    uint8_t code = text[j];
    if (code >= firstChar) {
      uint8_t charCode = code - firstChar;

      // 4 Bytes per char code
      uint8_t msbJumpToChar    = pgm_read_byte( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES );                  // MSB  \ JumpAddress
      uint8_t lsbJumpToChar    = pgm_read_byte( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);   // LSB /
      uint8_t charByteSize     = pgm_read_byte( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);  // Size
      uint8_t currentCharWidth = pgm_read_byte( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH); // Width

      // Test if the char is drawable
      if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
        // Get the position of the char data
        uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);
        drawInternal(xPos, yPos, currentCharWidth, textHeight, fontData, charDataPosition, charByteSize);
      }

      cursorX += currentCharWidth;
    }
  }
}


void ScreenDisplay::drawString(int16_t xMove, int16_t yMove, String strUser) {
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

  // char* text must be freed!
  char* text = utf8ascii(strUser);

  uint16_t yOffset = 0;
  // If the string should be centered vertically too
  // we need to now how heigh the string is.
  if (textAlignment == TEXT_ALIGN_CENTER_BOTH) {
    uint16_t lb = 0;
    // Find number of linebreaks in text
    for (uint16_t i=0;text[i] != 0; i++) {
      lb += (text[i] == 10);
    }
    // Calculate center
    yOffset = (lb * lineHeight) >> 1;
  }

  uint16_t line = 0;
  char* textPart = strtok(text,"\n");
  while (textPart != NULL) {
    uint16_t length = strlen(textPart);
    drawStringInternal(xMove, yMove - yOffset + (line++) * lineHeight, textPart, length, getStringWidth(textPart, length));
    textPart = strtok(NULL, "\n");
  }
  free(text);
}

void ScreenDisplay::drawStringMaxWidth(int16_t xMove, int16_t yMove, uint16_t maxLineWidth, String strUser) {
  uint16_t firstChar  = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

  char* text = utf8ascii(strUser);

  uint16_t length = strlen(text);
  uint16_t lastDrawnPos = 0;
  uint16_t lineNumber = 0;
  uint16_t strWidth = 0;

  uint16_t preferredBreakpoint = 0;
  uint16_t widthAtBreakpoint = 0;

  for (uint16_t i = 0; i < length; i++) {
    strWidth += pgm_read_byte(fontData + JUMPTABLE_START + (text[i] - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

    // Always try to break on a space or dash
    if (text[i] == ' ' || text[i]== '-') {
      preferredBreakpoint = i;
      widthAtBreakpoint = strWidth;
    }

    if (strWidth >= maxLineWidth) {
      if (preferredBreakpoint == 0) {
        preferredBreakpoint = i;
        widthAtBreakpoint = strWidth;
      }
      drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight , &text[lastDrawnPos], preferredBreakpoint - lastDrawnPos, widthAtBreakpoint);
      lastDrawnPos = preferredBreakpoint + 1;
      // It is possible that we did not draw all letters to i so we need
      // to account for the width of the chars from `i - preferredBreakpoint`
      // by calculating the width we did not draw yet.
      strWidth = strWidth - widthAtBreakpoint;
      preferredBreakpoint = 0;
    }
  }

  // Draw last part if needed
  if (lastDrawnPos < length) {
    drawStringInternal(xMove, yMove + lineNumber * lineHeight , &text[lastDrawnPos], length - lastDrawnPos, getStringWidth(&text[lastDrawnPos], length - lastDrawnPos));
  }

  free(text);
}

uint16_t ScreenDisplay::getStringWidth(const char* text, uint16_t length) {
  uint16_t firstChar        = pgm_read_byte(fontData + FIRST_CHAR_POS);

  uint16_t stringWidth = 0;
  uint16_t maxWidth = 0;

  while (length--) {
    stringWidth += pgm_read_byte(fontData + JUMPTABLE_START + (text[length] - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);
    if (text[length] == 10) {
      maxWidth = max(maxWidth, stringWidth);
      stringWidth = 0;
    }
  }

  return max(maxWidth, stringWidth);
}

uint16_t ScreenDisplay::getStringWidth(String strUser) {
  char* text = utf8ascii(strUser);
  uint16_t length = strlen(text);
  uint16_t width = getStringWidth(text, length);
  free(text);
  return width;
}

void ScreenDisplay::setTextAlignment(DISPLAY_TEXT_ALIGNMENT textAlignment) {
  this->textAlignment = textAlignment;
}

void ScreenDisplay::setFont(const uint8_t *fontData) {
  this->fontData = fontData;
}

void ScreenDisplay::displayOn(void) {
  sendCommand(DISPLAYON);
}

void ScreenDisplay::displayOff(void) {
  sendCommand(DISPLAYOFF);
}

void ScreenDisplay::invertDisplay(void) {
  sendCommand(INVERTDISPLAY);
}

void ScreenDisplay::normalDisplay(void) {
  sendCommand(NORMALDISPLAY);
}

void ScreenDisplay::setContrast(uint8_t contrast, uint8_t precharge, uint8_t comdetect) {
  sendCommand(SETPRECHARGE); //0xD9
  sendCommand(precharge); //0xF1 default, to lower the contrast, put 1-1F
  sendCommand(SETCONTRAST);
  sendCommand(contrast); // 0-255
  sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  sendCommand(comdetect);	//0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(DISPLAYON);
}

void ScreenDisplay::setBrightness(uint8_t brightness) {
  uint8_t contrast = brightness;
  if (brightness < 128) {
    // Magic values to get a smooth/ step-free transition
    contrast = brightness * 1.171;
  } else {
    contrast = brightness * 1.171 - 43;
  }

  uint8_t precharge = 241;
  if (brightness == 0) {
    precharge = 0;
  }
  uint8_t comdetect = brightness / 8;

  setContrast(contrast, precharge, comdetect);
}

void ScreenDisplay::resetOrientation() {
   screenRotate(ANGLE_0_DEGREE);
}

void ScreenDisplay::screenRotate(DISPLAY_ANGLE angle)
{
	this->rotate_angle=angle;
	if(this->rotate_angle==ANGLE_90_DEGREE||this->rotate_angle==ANGLE_270_DEGREE)
	{
		switch (this->geometry) {
		case GEOMETRY_128_64:
			this->displayWidth = 64;
			this->displayHeight = 128;
			break;
		case GEOMETRY_128_32:
			this->displayWidth = 32;
			this->displayHeight = 128;
			break;
		case GEOMETRY_200_200:
			this->displayWidth = 200;
			this->displayHeight = 200;
			break;
		case GEOMETRY_296_128:
			this->displayWidth = 128;
			this->displayHeight = 296;
			break;
		case GEOMETRY_250_122:
			this->displayWidth = 128;
			this->displayHeight = 256;
			break;
		case GEOMETRY_RAWMODE:
			this->displayWidth = 64;
			this->displayHeight = 128;
			break;
		}
	}
	else
	{
		switch (this->geometry) {
		case GEOMETRY_128_64:
			this->displayWidth = 128;
			this->displayHeight = 64;
			break;
		case GEOMETRY_128_32:
			this->displayWidth = 128;
			this->displayHeight = 32;
			break;
		case GEOMETRY_200_200:
			this->displayWidth = 200;
			this->displayHeight = 200;
			break;
		case GEOMETRY_296_128:
			this->displayWidth = 296;
			this->displayHeight = 128;
			break;
		case GEOMETRY_250_122:
			this->displayWidth = 256;
			this->displayHeight = 128;
			break;
		case GEOMETRY_RAWMODE:
			this->displayWidth = 128;
			this->displayHeight = 64;
			break;
		}
	}
	sendScreenRotateCommand();
}

void ScreenDisplay::resetScreenRotate()
{
	screenRotate(ANGLE_0_DEGREE);
}

void ScreenDisplay::flipScreenVertically() {
	screenRotate(ANGLE_180_DEGREE);
}

void ScreenDisplay::clear(void) {
  memset(buffer, 0, displayBufferSize);
}

void ScreenDisplay::drawLogBuffer(uint16_t xMove, uint16_t yMove) {
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);
  // Always align left
  setTextAlignment(TEXT_ALIGN_LEFT);

  // State values
  uint16_t length   = 0;
  uint16_t line     = 0;
  uint16_t lastPos  = 0;

  for (uint16_t i=0;i<this->logBufferFilled;i++){
    // Everytime we have a \n print
    if (this->logBuffer[i] == 10) {
      length++;
      // Draw string on line `line` from lastPos to length
      // Passing 0 as the lenght because we are in TEXT_ALIGN_LEFT
      drawStringInternal(xMove, yMove + (line++) * lineHeight, &this->logBuffer[lastPos], length, 0);
      // Remember last pos
      lastPos = i;
      // Reset length
      length = 0;
    } else {
      // Count chars until next linebreak
      length++;
    }
  }
  // Draw the remaining string
  if (length > 0) {
    drawStringInternal(xMove, yMove + line * lineHeight, &this->logBuffer[lastPos], length, 0);
  }
}

uint16_t ScreenDisplay::getWidth(void) {
  return displayWidth;
}

uint16_t ScreenDisplay::getHeight(void) {
  return displayHeight;
}

bool ScreenDisplay::setLogBuffer(uint16_t lines, uint16_t chars){
  if (logBuffer != NULL) free(logBuffer);
  uint16_t size = lines * chars;
  if (size > 0) {
    this->logBufferLine     = 0;      // Lines printed
    this->logBufferFilled   = 0;      // Nothing stored yet
    this->logBufferMaxLines = lines;  // Lines max printable
    this->logBufferSize     = size;   // Total number of characters the buffer can hold
    this->logBuffer         = (char *) malloc(size * sizeof(uint8_t));
    if(!this->logBuffer) {
      DEBUG_DISPLAY("[DISPLAY][setLogBuffer] Not enough memory to create log buffer\n");
      return false;
    }
  }
  return true;
}

size_t ScreenDisplay::write(uint8_t c) {
  if (this->logBufferSize > 0) {
    // Don't waste space on \r\n line endings, dropping \r
    if (c == 13) return 1;

    // convert UTF-8 character to font table index
    c = (this->fontTableLookupFunction)(c);
    // drop unknown character
    if (c == 0) return 1;

    bool maxLineNotReached = this->logBufferLine < this->logBufferMaxLines;
    bool bufferNotFull = this->logBufferFilled < this->logBufferSize;

    // Can we write to the buffer?
    if (bufferNotFull && maxLineNotReached) {
      this->logBuffer[logBufferFilled] = c;
      this->logBufferFilled++;
      // Keep track of lines written
      if (c == 10) this->logBufferLine++;
    } else {
      // Max line number is reached
      if (!maxLineNotReached) this->logBufferLine--;

      // Find the end of the first line
      uint16_t firstLineEnd = 0;
      for (uint16_t i=0;i<this->logBufferFilled;i++) {
        if (this->logBuffer[i] == 10){
          // Include last char too
          firstLineEnd = i + 1;
          break;
        }
      }
      // If there was a line ending
      if (firstLineEnd > 0) {
        // Calculate the new logBufferFilled value
        this->logBufferFilled = logBufferFilled - firstLineEnd;
        // Now we move the lines infront of the buffer
        memcpy(this->logBuffer, &this->logBuffer[firstLineEnd], logBufferFilled);
      } else {
        // Let's reuse the buffer if it was full
        if (!bufferNotFull) {
          this->logBufferFilled = 0;
        }// else {
        //  Nothing to do here
        //}
      }
      write(c);
    }
  }
  // We are always writing all uint8_t to the buffer
  return 1;
}

size_t ScreenDisplay::write(const char* str) {
  if (str == NULL) return 0;
  size_t length = strlen(str);
  for (size_t i = 0; i < length; i++) {
    write(str[i]);
  }
  return length;
}


// Private functions
void ScreenDisplay::setGeometry(DISPLAY_GEOMETRY g, uint16_t width, uint16_t height) {
	this->geometry = g;
	switch (g) {
		case GEOMETRY_128_64:
			this->displayWidth = 128;
			this->displayHeight = 64;
			break;
		case GEOMETRY_128_32:
			this->displayWidth = 128;
			this->displayHeight = 32;
			break;
		case GEOMETRY_200_200:
			this->displayWidth = 200;
			this->displayHeight = 200;
			break;
		case GEOMETRY_296_128:
			this->displayWidth = 296;
			this->displayHeight = 128;
			break;
		case GEOMETRY_250_122:
			this->displayWidth = 256;
			this->displayHeight = 128;
			break;
		case GEOMETRY_RAWMODE:
			this->displayWidth = width > 0 ? width : 128;
			this->displayHeight = height > 0 ? height : 64;
			break;
	}
  this->displayBufferSize = displayWidth * displayHeight /8;
}

void ScreenDisplay::setRst(int8_t rst)
{
	this->rst = rst;
}

void inline ScreenDisplay::drawInternal(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *data, uint16_t offset, uint16_t bytesInData) {
  if (width < 0 || height < 0) return;
  if (yMove + height < 0 || yMove > this->height())  return;
  if (xMove + width  < 0 || xMove > this->width())   return;

  uint8_t  rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
  int8_t   yOffset      = yMove & 7;

  bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

  int16_t initYMove   = yMove;
  int8_t  initYOffset = yOffset;


  for (uint16_t i = 0; i < bytesInData; i++) {

    // Reset if next horizontal drawing phase is started.
    if ( i % rasterHeight == 0) {
      yMove   = initYMove;
      yOffset = initYOffset;
    }

    uint8_t currentByte = pgm_read_byte(data + offset + i);

    int16_t xPos = xMove + (i / rasterHeight);
    int16_t yPos = ((yMove >> 3) + (i % rasterHeight)) * this->width();

//    int16_t yScreenPos = yMove + yOffset;
    int16_t dataPos    = xPos  + yPos;

    if (dataPos >=  0  && dataPos < displayBufferSize &&
        xPos    >=  0  && xPos    < this->width() ) {

      if (yOffset >= 0) {
        switch (this->color) {
          case WHITE:   buffer[dataPos] |= currentByte << yOffset; break;
          case BLACK:   buffer[dataPos] &= ~(currentByte << yOffset); break;
          case INVERSE: buffer[dataPos] ^= currentByte << yOffset; break;
        }

        if (dataPos < (displayBufferSize - this->width())) {
          switch (this->color) {
            case WHITE:   buffer[dataPos + this->width()] |= currentByte >> (8 - yOffset); break;
            case BLACK:   buffer[dataPos + this->width()] &= ~(currentByte >> (8 - yOffset)); break;
            case INVERSE: buffer[dataPos + this->width()] ^= currentByte >> (8 - yOffset); break;
          }
        }
      } else {
        // Make new offset position
        yOffset = -yOffset;

        switch (this->color) {
          case WHITE:   buffer[dataPos] |= currentByte >> yOffset; break;
          case BLACK:   buffer[dataPos] &= ~(currentByte >> yOffset); break;
          case INVERSE: buffer[dataPos] ^= currentByte >> yOffset; break;
        }

        // Prepare for next iteration by moving one block up
        yMove -= 8;

        // and setting the new yOffset
        yOffset = 8 - yOffset;
      }
    }
  }
}

void ScreenDisplay::sleep() {
	sendCommand(0x8D);
	sendCommand(0x10);
	sendCommand(0xAE);
}

void ScreenDisplay::wakeup() {
	sendCommand(0x8D);
	sendCommand(0x14);
	sendCommand(0xAF);
}


// You need to free the char!
char* ScreenDisplay::utf8ascii(String str) {
  uint16_t k = 0;
  uint16_t length = str.length() + 1;

  // Copy the string into a char array
  char* s = (char*) malloc(length * sizeof(char));
  if(!s) {
    DEBUG_DISPLAY("[DISPLAY][utf8ascii] Can't allocate another char array. Drop support for UTF-8.\n");
    return (char*) str.c_str();
  }
  str.toCharArray(s, length);

  length--;

  for (uint16_t i=0; i < length; i++) {
    char c = (this->fontTableLookupFunction)(s[i]);
    if (c!=0) {
      s[k++]=c;
    }
  }

  s[k]=0;

  // This will leak 's' be sure to free it in the calling function.
  return s;
}

void ScreenDisplay::setFontTableLookupFunction(FontTableLookupFunction function) {
  this->fontTableLookupFunction = function;
}


char DefaultFontTableLookup(const uint8_t ch) {
    // UTF-8 to font table index converter
    // Code form http://playground.arduino.cc/Main/Utf8ascii
	static uint8_t LASTCHAR;

	if (ch < 128) { // Standard ASCII-set 0..0x7F handling
		LASTCHAR = 0;
		return ch;
	}

	uint8_t last = LASTCHAR;   // get last char
	LASTCHAR = ch;

	switch (last) {    // conversion depnding on first UTF8-character
		case 0xC2: return (uint8_t) ch;
		case 0xC3: return (uint8_t) (ch | 0xC0);
		case 0x82: if (ch == 0xAC) return (uint8_t) 0x80;    // special case Euro-symbol
	}

	return (uint8_t) 0; // otherwise: return zero, if character has to be ignored
}
