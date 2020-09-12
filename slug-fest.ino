#define NORMAL_ATTACK_DURATION 100
#define TURN_DURATION_PER_PLAYER 500
#define ATTACK_ANIM_DURATION 100
#define LIGHT_BLUE makeColorRGB(173,216,230)

enum dataMode{
  ACK_IDLE = 0,
  SLUG_DATA = 1,
  TOGGLE_TURN = 2,
  SETUP_RESET = 3
};

enum playerClass{
  MUSHROOM = 0,
  SLUG  = 1,
  UNASSIGNED = 2
};

Timer mainTimer;
Timer secondaryTimer;

uint8_t playerType = UNASSIGNED;
uint8_t health = 4;
uint8_t awayFace, toCenterFace = FACE_COUNT;
uint8_t animCount = 0, attackTypeCounter;

uint8_t startFace;
uint8_t holdData;

byte team1Total = 0, team2Total, stepRange = 0;
byte attackStepBrightness = MAX_BRIGHTNESS;

uint8_t winPassCount = 0, winAnimCount = 0;
uint8_t winPosition = 0;

bool hasHealed;
bool isActive, isAttacking, isHit, isError, isHealing, isWinning;
bool team1Turn = true;

void setup() {
   setColor(WHITE);
}

void loop() {
  //**Button clicks**
  if (buttonSingleClicked() && !hasWoken()){
    //if active
    //attack sequence
    if (isActive){
      if (health >0){// && playerType == SLUG){
        //change this later
        if (isAttacking){
          uint8_t range = stepRange;
          isAttacking = false;
          stepRange = team1Total-1;
          mainTimer.set(0);
          secondaryTimer.set(0);
          attackStepBrightness = MAX_BRIGHTNESS;
          boolean crit = attackTypeCounter == 2;
          uint8_t dataLoad = (range<<1)+(crit?1:0);
          setValueSentOnFace((dataLoad<<2)+SLUG_DATA, toCenterFace);
        }else{
          if (!isValueReceivedOnFaceExpired(awayFace)){
            setValueSentOnFace((1<<2)+ACK_IDLE, awayFace);
          }
          setValueSentOnFace((1<<2)+ACK_IDLE, toCenterFace);
          attackTypeCounter = 0;
          mainTimer.set(NORMAL_ATTACK_DURATION*health*team1Total);
          attackStepBrightness = MAX_BRIGHTNESS;
          stepRange = team1Total-1;
          secondaryTimer.set(NORMAL_ATTACK_DURATION*health);
          isAttacking = true;
        }
      }
    }
  }
  
  //Made this a 3+ click since it's possible to double click on accident during the attack. you wouldn't want to rest on accident.
  if (buttonMultiClicked() && !hasWoken()){
    setUpGame();
  }

//  if (buttonDoubleClicked()){
//    sendToggle(toCenterFace, 7, true);
//  }

  if (buttonLongPressed() && !hasWoken() && isActive){
    //begin test if game can start
    if (playerType != MUSHROOM && !hasHealed && (health>0 && health <=2)){
      healUp();
    }else if (playerType != MUSHROOM){
      showError();
    }
  }
  //**END Button clicks**

  //**START Animations**
  if (isHit){
    if(mainTimer.isExpired()){
      mainTimer.set(ATTACK_ANIM_DURATION*2);
    }
    gameAnim((holdData&4)==4?RED:ORANGE);
  }
  
  if (isError){
    if(mainTimer.isExpired()){
      mainTimer.set(ATTACK_ANIM_DURATION*2);
      attackTypeCounter -=1;
    }
    gameAnim(RED);
    if (attackTypeCounter ==0){
        isError= false;
        refreshAllFaces();
    }
  }
  if (isHealing){
    if(mainTimer.isExpired()){
      mainTimer.set(ATTACK_ANIM_DURATION*5);
      attackTypeCounter -=1;
    }
    gameAnim(GREEN);
    if (attackTypeCounter ==0){
        isHealing= false;
        hasHealed=true;
        health+=2;
        refreshAllFaces();
    }
  }

 if(isWinning){
    if (winPassCount < 3){
      bool endOfLine = (isValueReceivedOnFaceExpired(awayFace) || (winPassCount<2 && startFace == awayFace && winPosition==1));
      if (mainTimer.isExpired()){
        if (!endOfLine && winAnimCount == 4){
          winAnimCount=7;
        }
        winAnim();
       if (winAnimCount == 7){
          isWinning = false;
          if (startFace == toCenterFace){
            if (!isValueReceivedOnFaceExpired(awayFace)){
              setValueSentOnFace(holdData, awayFace);
            }else{
              winPassCount +=1;
              setValueSentOnFace(holdData, toCenterFace);
              if (winPassCount==3){
                youWon();
              }
            }
          }else{
            winPassCount +=1;
             if (winPosition>1){
              if (winPassCount==3){
                youWon();
              }
               setValueSentOnFace(holdData, toCenterFace);
             }else{
               if (winPassCount<3){
                  setValueSentOnFace(holdData, awayFace);
               }
               if (winPassCount==3){
                youWon();
                setValueSentOnFace(ACK_IDLE, toCenterFace);
               }
             }
             
           }
           winAnimCount = 0;
       }else{
        winAnimCount+=1;
        mainTimer.set(ATTACK_ANIM_DURATION/5);
       }
      }
    }
 }
  
  
 if(playerType == MUSHROOM && isAttacking){
   if (mainTimer.isExpired()){
      //toggleTurns
      handleNewTurn(false);
      isAttacking = false;
    }
  }

 
 if(isAttacking && playerType != MUSHROOM) {
   if(mainTimer.isExpired()) {
       if(!hasHealed && attackTypeCounter<2){
         attackTypeCounter +=1;
       }else{
          attackTypeCounter = 0;
       }
       uint8_t critMod = (attackTypeCounter == 2)? 2:1;
       mainTimer.set((NORMAL_ATTACK_DURATION*health*team1Total)/critMod);
     }
     if(secondaryTimer.isExpired()) {
      if(stepRange>0) {
        attackStepBrightness -= (MAX_BRIGHTNESS/team1Total);
        stepRange -=1;
      }else{
        //start over
        attackStepBrightness = MAX_BRIGHTNESS;
        stepRange = team1Total-1;
      }
      uint8_t critMod = (!hasHealed && attackTypeCounter == 2)? 2:1;
      secondaryTimer.set((NORMAL_ATTACK_DURATION*health)/critMod);
    }
  }

  if (animCount>0){
    if (secondaryTimer.isExpired()){
      animCount-=1;
      uint8_t oppositeFace = (startFace+3)%FACE_COUNT;
      if (animCount==2){
        setFaceColor(startFace, YELLOW);
        secondaryTimer.set(ATTACK_ANIM_DURATION);
      }else if(animCount==1){
        setFaceColor(oppositeFace,YELLOW);
        if (playerType == MUSHROOM){
          setFaceColor(startFace, GREEN);
        }else{
          setFaceColor(startFace, isActive?LIGHT_BLUE:OFF);
        }
        secondaryTimer.set(ATTACK_ANIM_DURATION);
      }else{
        if (playerType == MUSHROOM){
          setFaceColor(oppositeFace, RED);
          isAttacking=true;
          mainTimer.set(2000+(TURN_DURATION_PER_PLAYER*(startFace==awayFace?team1Total:team2Total)));
        }else{
          setFaceColor((startFace+3)%FACE_COUNT, isActive?WHITE:OFF);
        }
        setValueSentOnFace(ACK_IDLE, startFace);
        setValueSentOnFace(holdData, oppositeFace);
      }
    }
  }
 
 if(isAttacking && playerType != MUSHROOM){
    setFaceColor(toCenterFace, dim((!hasHealed && attackTypeCounter == 2)?RED:WHITE, attackStepBrightness));
 }
//**END Animations**

//**DATA handler**
  FOREACH_FACE(face){
     if (didValueOnFaceChange(face)){
       parseData(getLastValueReceivedOnFace(face), face);
     }
  }
}

//***Main data switch***
void parseData(uint8_t data, int faceOfSignal){
    uint8_t inDataMode = data & 3;
    uint8_t dataLoad = data >> 2;
    boolean firstLoadBit = (dataLoad & 1) == 1;
    uint8_t restBits = dataLoad >> 1;
    switch(inDataMode){
      case ACK_IDLE:
        handleIdle(firstLoadBit, faceOfSignal == toCenterFace);
        break;
      case SLUG_DATA:
        handleAttack(faceOfSignal, restBits, firstLoadBit);
        break;
      case TOGGLE_TURN:
        handleToggle(restBits, firstLoadBit, faceOfSignal == toCenterFace);
        break;
      case SETUP_RESET:
        handleSetupReset(firstLoadBit, restBits, faceOfSignal);
        break;
    }
}

//***BEGIN Data Handlers***
void handleIdle(boolean isOtherAttacking, boolean isFromCenter){
  
  if (isOtherAttacking){
    if (playerType != MUSHROOM){
      isAttacking=false;
      refreshAllFaces();
      if(isFromCenter){
        if (!isValueReceivedOnFaceExpired(awayFace)){
          setValueSentOnFace((1<<2)+ACK_IDLE, awayFace);
        }
      }else{
        setValueSentOnFace((1<<2)+ACK_IDLE, toCenterFace);
      }
    }
  }
  setValueSentOnFace(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
}

void handleToggle(uint8_t headCount, boolean aWinnerIsYou, boolean isFromCenter){
  setValueSentOnFace(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
  if (playerType == MUSHROOM){
      if (headCount == 0){
        //handle win condition
        setFaceColor(isFromCenter?awayFace:toCenterFace, WHITE);
        sendToggle(isFromCenter?awayFace:toCenterFace, 7, true);
      }
    }else{
      if (aWinnerIsYou){
          holdData = (1<<2)+TOGGLE_TURN;
          startFace=isFromCenter?toCenterFace:awayFace;
          isWinning = true;
          winAnimCount =0;
          setValueSentOnFace(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
      }else{
        if (isFromCenter){
          
          //Using team1Total on non-mushroom to track enemy team size
          team1Total = headCount;
          if(health>0){
            isActive = !isActive;
          }else{
            isActive = false;
          }
          refreshAllFaces();
          if (!isValueReceivedOnFaceExpired(awayFace)){
            sendToggle(awayFace, headCount, false);
            //setValueSentOnFace(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
          }else{
            sendToggle(toCenterFace, (health>0)?1:0, false);
          }
        }else{
          if(health>0){
              //calculate the numAlive
              headCount+=1;
          }
          sendToggle(toCenterFace, headCount, false);
          //setValueSentOnFace(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
       }
    }
  }
}

void sendToggle(int face, uint8_t aliveCount, bool aWinnerIsYou){
  uint8_t dataLoad = (aliveCount<<1)+(aWinnerIsYou?1:0);
  setValueSentOnFace((dataLoad<<2)+TOGGLE_TURN, face);
}

void handleSetupReset(boolean onWayOut, uint8_t blinkCount, int faceOfSignal){
  setValueSentOnFace(ACK_IDLE, faceOfSignal);
  int oppositeFace = (faceOfSignal+3)%FACE_COUNT;
  if (onWayOut){
    toCenterFace = faceOfSignal;
    awayFace = oppositeFace;
  }
 
  if (playerType == MUSHROOM && !onWayOut){
    
    if (faceOfSignal == awayFace){
      team1Total = blinkCount;
    }else if (faceOfSignal == toCenterFace){
      team2Total = blinkCount;
    }
    if (team2Total > 0 && team1Total > 0){
        beginGame();
    }
  }else{
     
    if (onWayOut){
      winPosition = blinkCount;
      setPlayerType(SLUG);
      if (!isValueReceivedOnFaceExpired(awayFace)){
        uint8_t dataLoad = ((blinkCount+1)<<1)+1;
        setValueSentOnFace((dataLoad<<2)+SETUP_RESET, awayFace);
      }else{
        //pass it back
        setValueSentOnFace((blinkCount<<3)+SETUP_RESET, toCenterFace);
      }
    }else{
      setValueSentOnFace((blinkCount<<3)+SETUP_RESET, toCenterFace);
    }
  }
  
}

void handleAttack(int faceOfSignal, uint8_t range, boolean crit){
  if (playerType == MUSHROOM){
        animCount=3;
    }else{
      if (faceOfSignal == toCenterFace){
        if (range>0){
          range -= 1;
          animCount=3;
        }else{
          isHit=true;
          if (crit && health >1){
            health -=2;
          }else if (health >0){
            health -=1;
          }else {
            health = 0;
          }
        }
      }else{
        animCount=3;
      }
  }
  uint8_t dataLoad = ((range<<1)+(crit?1:0));
  holdData = (dataLoad<<2)+SLUG_DATA;
  startFace=faceOfSignal;
}
//**END Data Handlers***
//***BEGIN Utils***

void winAnim(){
  setFaceColor(startFace, (winAnimCount==0||winAnimCount==6)?GREEN:OFF);
  setFaceColor((startFace+1)%FACE_COUNT, (winAnimCount==1||winAnimCount==5)?GREEN:OFF);
  setFaceColor((startFace+5)%FACE_COUNT, (winAnimCount==1||winAnimCount==5)?GREEN:OFF);
  setFaceColor((startFace+2)%FACE_COUNT, (winAnimCount==2||winAnimCount==4)?GREEN:OFF);
  setFaceColor((startFace+4)%FACE_COUNT, (winAnimCount==2||winAnimCount==4)?GREEN:OFF);
  setFaceColor((startFace+3)%FACE_COUNT, winAnimCount==3?GREEN:OFF);
}

void youWon(){
  winAnimCount = 0;
  setColor(GREEN);
  isWinning = false;
  winPassCount = 0;
}

void gameAnim(Color color){
  setColor(dim(color, MAX_BRIGHTNESS - mainTimer.getRemaining()));
}

void refreshAllFaces(){
  isHit=false;
  
  setFaceColor((awayFace+1)%FACE_COUNT,health>0?GREEN:RED);
  setFaceColor((awayFace+2)%FACE_COUNT,health>1?GREEN:RED);
  setFaceColor((awayFace+4)%FACE_COUNT,health>2?GREEN:RED);
  setFaceColor((awayFace+5)%FACE_COUNT,health>3?GREEN:RED);
  
  setFaceColor(awayFace, isActive?LIGHT_BLUE:OFF);
  setFaceColor(toCenterFace, isActive?WHITE:OFF);
}

void handleNewTurn(boolean isFromSetup){
  team1Turn = !team1Turn;
  if (team1Turn){
    if (!isFromSetup){
      setValueSentOnFace((team1Total<<3)+TOGGLE_TURN, toCenterFace);
    }
    setValueSentOnFace((team2Total<<3)+TOGGLE_TURN, awayFace);
    setFaceColor(awayFace,GREEN);
    setFaceColor(toCenterFace,RED);
  }else{
    if (!isFromSetup){
      setValueSentOnFace((team2Total<<3)+TOGGLE_TURN, awayFace);
    }
    setValueSentOnFace((team1Total<<3)+TOGGLE_TURN, toCenterFace);
    setFaceColor(awayFace,RED);
    setFaceColor(toCenterFace, GREEN);
  }
}

void beginGame(){
  //spin to one side or the other
  randomize();
  int selectedTeam = random(1000);
  team1Turn = selectedTeam < 500;
  handleNewTurn(true);
}

void setPlayerType(uint8_t playerClass){
  playerType = playerClass;
  winPassCount = 0;
  isActive= hasHealed = isAttacking= isHit= isError= isHealing= isWinning = false;
  switch(playerType){
    case SLUG:
      health = 4;
      refreshAllFaces();
      break;
    case MUSHROOM:
      setColor(WHITE);
      health = 0;
      break;
  }
}

void setUpGame(){
  team1Total = team2Total = 0;
  toCenterFace = awayFace = FACE_COUNT;
  setPlayerType(MUSHROOM);
  FOREACH_FACE(face){
    //check if end
    if (!isValueReceivedOnFaceExpired(face)){
      if (toCenterFace == FACE_COUNT){
        toCenterFace = face;
      }else{
        awayFace = face;
      }
      setValueSentOnFace((3<<2)+SETUP_RESET,face);
    }
  }
}

 void showError(){
    isError = true;
    attackTypeCounter = 3;
 }

 void healUp(){
    isHealing = true;
    attackTypeCounter = 3;
 }
 //***END Utils***
