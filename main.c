#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <fcntl.h>
#include "ili9340.h"


#define _DEBUG_ 1
FontxFile fx32G[2];
FontxFile fx24G[2];
FontxFile fx16G[2];
FontxFile fx32M[2];
FontxFile fx24M[2];
FontxFile fx16M[2];

double lastBTCPrice = 0.0;
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


void readCreds(char* credp, ssize_t size){
   int fd = open("./creds", O_RDONLY);
   if(fd > 0) {
	read(fd, credp, size);
	close(fd);
   }
}

void getJWT(char* cred, struct MemoryStruct *chunk) {
   char post_body[64];
   CURL *curl;
   CURLcode res;
   sprintf(post_body, "{\"password\": \"%s\"}", cred);
   if(_DEBUG_) printf("POST body: %s\n", post_body);

   curl_global_init(CURL_GLOBAL_ALL);
   curl = curl_easy_init();
   if(curl) {
     curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
     curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000/v1/accounts/login");
     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
     curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
     struct curl_slist *headers = NULL;
     headers = curl_slist_append(headers, "Content-Type: application/json");
     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
     curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
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
}

void parseJWT(char* rawJson, char* parsedJWT) {
     struct json_object *parsed_json;
     struct json_object *jwt_object;
     
     parsed_json = json_tokener_parse(rawJson);
     (json_object_object_get_ex(parsed_json, "jwt", &jwt_object));
     char* jwtCopy = json_object_to_json_string_ext(jwt_object, 0);
     strcpy(parsedJWT, jwtCopy); 
}

void getBlockHeight(char *jwt,  struct MemoryStruct *chunk){
     CURL *curl;
     curl = curl_easy_init();
     char jwtAuthHeader[502];
	 sprintf(jwtAuthHeader,"Authorization:JWT%s", jwt);
	 printf("Header %s \n", jwtAuthHeader);
	 if(curl) {
	    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3002/v1/bitcoind/info/sync");
	    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
	    headers = curl_slist_append(headers, jwtAuthHeader);
		headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        /* send all data to this function  */ 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        /* we pass our 'chunk' struct to the callback function */ 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
        curl_easy_perform(curl);
	}
     curl_easy_cleanup(curl);
}


void parseBlockHeight(char* rawJson, char* blockHeight) {
	 struct json_object *parsed_json;
     struct json_object *block_height_object;
    	 
     parsed_json = json_tokener_parse(rawJson);
     (json_object_object_get_ex(parsed_json, "currentBlock", &block_height_object));
	 char* blockHeightCopy = json_object_to_json_string_ext(block_height_object, 0);
	 strcpy(blockHeight, blockHeightCopy);
}

void printBlockHeight(char* height){
    lcdSetFontDirection(DIRECTION90);
    lcdDrawUTF8String(fx24G, 80, 300, "Block Height:", WHITE);
    lcdDrawUTF8String(fx32G, 80, 140, height, WHITE);
}

void displayBlockHeight() {
    unsigned int PASS_SIZE = 16;
    unsigned int JWT_SIZE = 188;
	unsigned int BLOCK_HEIGHT_SIZE = 32; 
    char cred[PASS_SIZE];
    char jwt[JWT_SIZE];
	char blockHeight[BLOCK_HEIGHT_SIZE];
    struct MemoryStruct chunk;
    chunk.memory = malloc(1); 
    chunk.size = 0; 

    readCreds(cred, PASS_SIZE);
    cred[PASS_SIZE - 1] = '\0'; 
    getJWT(cred, &chunk); 
    parseJWT(chunk.memory, jwt);
	jwt[0] = ' ';
	jwt[JWT_SIZE + 1] = '\0';
    chunk.memory = malloc(1); 
    chunk.size = 0; 
    printf("PARSE JWT %s\n", jwt );
	getBlockHeight(jwt, &chunk);
	parseBlockHeight(chunk.memory, blockHeight);
    printf("Block Height: %s \n", blockHeight);
 	printBlockHeight(blockHeight); 
	free(chunk.memory);
}

void displayPrice() {
 	struct MemoryStruct chunk;
	chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
    chunk.size = 0;    /* no data at this point */  
	
    getPrices(&chunk); 
      if(_DEBUG_){
      	  printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
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
      free(chunk.memory);
}


int main(int argc, char** argv) {
    int i;
    int screenWidth = 0;
    int screenHeight = 0;
    int offsetx = 0;
    int offsety = 0;
    char dir[128];
    char cpath[128];
   
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
    if(_DEBUG_){
		printf("ReadTFTConfig:screenWidth=%d height=%d\n",screenWidth, screenHeight);
   		printf("Your TFT resolution is %d x %d.\n",screenWidth, screenHeight);
	    printf("Your TFT offsetx    is %d.\n",offsetx);
	    printf("Your TFT offsety    is %d.\n",offsety);
	}

    // Init fonts and lcd screen 
    initFonts();
    lcdInit(screenWidth, screenHeight, offsetx, offsety);
    lcdReset();
    lcdSetup();

    printBTCLabel();
    for(;;){
		displayBlockHeight();
		displayPrice();
    	sleep(60);
    }	
    curl_global_cleanup();
    return 1;


}

