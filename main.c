#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "ili9340.h"


#define _DEBUG_ 1
FontxFile fx32G[2];
FontxFile fx24G[2];
FontxFile fx16G[2];
FontxFile fx32M[2];
FontxFile fx24M[2];
FontxFile fx16M[2];

struct MemoryStruct {
   char *memory;
   size_t size;
};
 
void initFonts(){
    Fontx_init(fx32G,"./fontx/ILGH32XB.FNT","./fontx/ILGZ32XB.FNT"); // 16x32Dot Gothic
    Fontx_init(fx24G,"./fontx/ILGH24XB.FNT","./fontx/ILGZ24XB.FNT"); // 12x24Dot Gothic
    Fontx_init(fx16G,"./fontx/ILGH16XB.FNT","./fontx/ILGZ16XB.FNT"); // 8x16Dot Gothic

    Fontx_init(fx32M,"./fontx/ILMH32XF.FNT","./fontx/ILMZ32XF.FNT"); // 16x32Dot Mincyo
    Fontx_init(fx24M,"./fontx/ILMH24XF.FNT","./fontx/ILMZ24XF.FNT"); // 12x24Dot Mincyo
    Fontx_init(fx16M,"./fontx/ILMH16XB.FNT","./fontx/ILMZ16XF.FNT"); // 8x16Dot Mincyo
}


void inputKey() {
  char ch;
  printf("Hit any key");
  scanf("%c",&ch);
}

int ReadTFTConfig(char *path, int *width, int *height, int *offsetx, int *offsety) {
  FILE *fp;
  char buff[128];
  
  fp = fopen(path,"r");
  if(fp == NULL) return 0;
  while (fgets(buff,128,fp) != NULL) {
    if (buff[0] == '#') continue;
    if (buff[0] == 0x0a) continue;
    if (strncmp(buff, "width=", 6) == 0) {
      sscanf(buff, "width=%d height=%d",width,height);
      if(_DEBUG_)printf("width=%d height=%d\n",*width,*height);
    } else if (strncmp(buff, "offsetx=", 8) == 0) {
	sscanf(buff, "offsetx=%d",offsetx);
	if(_DEBUG_)printf("offsetx=%d\n",*offsetx);
    } else if (strncmp(buff, "offsety=", 8) == 0) {
      sscanf(buff, "offsety=%d",offsety);
      if(_DEBUG_)printf("offsety=%d\n",*offsety);
    }
  }
  fclose(fp);
  return 1;
}

void printBTCLabel(){
    unsigned char testText[64];
    lcdFillScreen( BLACK);
    lcdSetFontDirection(DIRECTION90);
    strncpy((char *)testText, "BTC", sizeof(testText));
    lcdDrawUTF8String(fx32G, 200,200, testText, WHITE);
}

void printPrice(char* price){
    lcdSetFontDirection(DIRECTION90);
    lcdDrawUTF8String(fx32G, 120, 180, price, WHITE);
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
	     
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        /* out of memory! */ 
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }
		 
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
 
    return realsize;
}

void getPrices(struct MemoryStruct *chunk){
    //char* RESOURCE_URL = "https://min-api.cryptocompare.com/data/pricemulti?fsyms=BTC,ETH,ZEC,LTC,DAI&tsyms=USD";
   char* RESOURCE_URL = "https://api.coindesk.com/v1/bpi/currentprice.json"; 
   CURL *curl;
   CURLcode res;
      
   curl_global_init(CURL_GLOBAL_ALL);
   curl = curl_easy_init();
   if(curl) {
     curl_easy_setopt(curl, CURLOPT_URL, RESOURCE_URL );
     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
     curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.42.0"); 
     /* send all data to this function  */ 
     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
      
     /* we pass our 'chunk' struct to the callback function */ 
     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
     
     res = curl_easy_perform(curl);
     /* Check for errors */ 
     if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
     /* always cleanup */ 
     curl_easy_cleanup(curl);				       
   }
   

   if(_DEBUG_) printf("Request made successfully ! \n");
}

char* getCryptoPrice(char* rawJson, char* cryptoSymbol, char* currency) {
     struct json_object *parsed_json;
     struct json_object *btc_object;
     struct json_object *currency_object;
     struct json_object *double_object;
     
     parsed_json = json_tokener_parse(rawJson);
     (json_object_object_get_ex(parsed_json, "bpi", &btc_object));
     (json_object_object_get_ex(btc_object, currency, &currency_object));
     (json_object_object_get_ex(currency_object, "rate_float", &double_object));

 
     if(_DEBUG_) printf("Current %s price:\n---\n%s\n---\n", currency, json_object_to_json_string_ext(double_object, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
     
     return json_object_to_json_string_ext(double_object, 0);

}

int main(int argc, char** argv) {

    int i;
    int screenWidth = 0;
    int screenHeight = 0;
    int offsetx = 0;
    int offsety = 0;
    char dir[128];
    char cpath[128];
    struct MemoryStruct chunk;
    double lastBTCPrice = 0.0;

    if(_DEBUG_)  printf("argv[0]=%s\n",argv[0]);
    strcpy(dir, argv[0]);
    for(i=strlen(dir);i>0;i--) {
        if (dir[i-1] == '/') {
          dir[i] = 0;
          break;
        } // end if
    } // end for
    if(_DEBUG_)printf("dir=%s\n",dir);
    strcpy(cpath,dir);
    strcat(cpath,"tft.conf");
    if(_DEBUG_)printf("cpath=%s\n",cpath);
    if (ReadTFTConfig(cpath, &screenWidth, &screenHeight, &offsetx, &offsety) == 0) {
        printf("%s Not found\n",cpath);
        return 0;
    }
    if(_DEBUG_)printf("ReadTFTConfig:screenWidth=%d height=%d\n",screenWidth, screenHeight);
    printf("Your TFT resolution is %d x %d.\n",screenWidth, screenHeight);
    printf("Your TFT offsetx    is %d.\n",offsetx);
    printf("Your TFT offsety    is %d.\n",offsety);


    // Init fonts and lcd screen 
    initFonts();
    lcdInit(screenWidth, screenHeight, offsetx, offsety);
    lcdReset();
    lcdSetup();
    for(;;){
       
       chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
       chunk.size = 0;    /* no data at this point */  

	
      printBTCLabel();
      getPrices(&chunk); 
      printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
      if(_DEBUG_){
          printf("Printing response: \n");
          for(int i=0;i<chunk.size; i++ ){
            printf("%c", chunk.memory[i]);
          }
   	  printf("\n");
      }

      //Extract USD - BTC Price
      char* btc_price_s = getCryptoPrice(chunk.memory, "BTC", "USD");
      printPrice(btc_price_s);
      // Convert to double 
      char * arbPtr;
      double btc_price_d = strtod(btc_price_s, &arbPtr);
      //Draw an arrow
      (lastBTCPrice > btc_price_d) 
	      ? lcdDrawFillArrow(120, 275, 150, 275, 20, GREEN) 
	      : lcdDrawFillArrow(150, 275, 120, 275, 20, RED);
      lastBTCPrice = btc_price_d;
      sleep(60);
      free(chunk.memory);
    }	
    curl_global_cleanup();
    return 1;


}

