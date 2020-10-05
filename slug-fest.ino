#ifndef BGA_CUSTOM_BLINKLIB
#error "This code requires a custom blinklib. See https://github.com/brunoga/blinklib/releases/latest"
#endif

#define NORMAL_ATTACK_DURATION 150
#define TURN_DURATION_PER_PLAYER 500
#define ATTACK_ANIM_DURATION 100
#define ANIM_DURATION_SHORT 20
#define ANIM_DURATION_MID 200
#define ANIM_DURATION_LONG 500
#define PULSE_TIMER_PER_HEALTH 637

byte faceOffsetArray[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };
#define CW_FROM_FACE(f, amt) faceOffsetArray[(f) + (amt)]
#define CCW_FROM_FACE(f, amt) faceOffsetArray[6 + (f) - (amt)]
#define OPPOSITE_FACE(f) CW_FROM_FACE((f), 3)

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

//some of these run simulataneously (plus consolodating doesn't seem to help memory much)
Timer pulseTimer, turnTimer, mainTimer, stepTimer;

uint8_t playerType = UNASSIGNED;
uint8_t health = 4;
uint8_t awayFace, toCenterFace = FACE_COUNT;
uint8_t animCount = 0, attackTypeCounter;

uint8_t startFace;
byte holdData;

byte team1AndOpponentTotal = 0, team2Total, stepRange = 0;
byte attackStepBrightness = MAX_BRIGHTNESS;

uint8_t winPassCount = 0, winAnimCount = 0;
uint8_t winPosition = 0;

bool hasHealed;
bool isActive, isAttacking, isHit, isError, isHealing, isWinning, hasWon;
bool team1Turn = true;

void setup() {
  setColor(WHITE);
}

void loop() {
  //**BEGIN Button clicks**
  if (buttonSingleClicked() && !hasWoken() && isActive && health >0){
    //attack sequence
    attackStepBrightness = MAX_BRIGHTNESS;
    if (isAttacking){
      uint8_t range = stepRange;
      isAttacking = false;
      mainTimer.set(0);
      stepTimer.set(0);
      stepRange = team1AndOpponentTotal-1;
      boolean crit = attackTypeCounter == 2;
      uint8_t dataLoad = (range<<1)+(crit?1:0);
      setValueSentOnFace((dataLoad<<2)+SLUG_DATA, toCenterFace);
    }else{
      int scaledDuration = NORMAL_ATTACK_DURATION*health;
      if (!isValueReceivedOnFaceExpired(awayFace)){
        setValueSentOnFace((1<<2)+ACK_IDLE, awayFace);
      }
      setValueSentOnFace((1<<2)+ACK_IDLE, toCenterFace);
      attackTypeCounter = 0;
      mainTimer.set(scaledDuration*team1AndOpponentTotal);
      stepRange = team1AndOpponentTotal-1;
      stepTimer.set(scaledDuration);
      isAttacking = true;
    }
  }
  
  //Made this a 3+ click since it's possible to double click on accident during the attack. you wouldn't want to reset on accident.
  if (buttonMultiClicked() && !hasWoken()){
    setUpGame();
  }

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
      mainTimer.set(ANIM_DURATION_MID);
    }
    gameAnim((holdData&4)==4?RED:ORANGE);
  }
  
  if (isError){
    if(mainTimer.isExpired()){
      mainTimer.set(ANIM_DURATION_MID);
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
      mainTimer.set(ANIM_DURATION_LONG);
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

  if (playerType == UNASSIGNED){
    if (mainTimer.isExpired()){
       mainTimer.set(2550);
    }
    setUpAnim();
  }

  if (pulseTimer.isExpired()){
    if (playerType == SLUG && health >0){
      pulseTimer.set(PULSE_TIMER_PER_HEALTH*health);
    }
  }else{
    if (playerType == SLUG && !(isWinning || isHit || isError || isHealing || hasWon)){
      refreshSides();
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
               else if (winPassCount==3){
                youWon();
                setValueSentOnFace(ACK_IDLE, toCenterFace);
               }
             }
           }
           winAnimCount = 0;
       }else{
        winAnimCount+=1;
        mainTimer.set(ANIM_DURATION_SHORT);
       }
      }
    }
 }
  
  if (turnTimer.isExpired() && playerType == MUSHROOM && isAttacking){
      //toggleTurns
      handleNewTurn(false);
      isAttacking = false;
  }

  
  if(isAttacking && playerType != MUSHROOM) {
    int scaledDuration = NORMAL_ATTACK_DURATION*health;
    if(mainTimer.isExpired()) {
       if(!hasHealed && attackTypeCounter<2){
         attackTypeCounter +=1;
       }else{
          attackTypeCounter = 0;
       }
       uint8_t critMod = (attackTypeCounter == 2)? 2:1;
       mainTimer.set((scaledDuration*team1AndOpponentTotal)/critMod);
       attackStepBrightness = MAX_BRIGHTNESS;
       stepRange = team1AndOpponentTotal-1;
       stepTimer.set((scaledDuration)/critMod);
    }
    if(stepTimer.isExpired()) {
      if(stepRange>0) {
        attackStepBrightness -= (MAX_BRIGHTNESS/team1AndOpponentTotal);
        stepRange -=1;
      }else{
        //start over
        attackStepBrightness = MAX_BRIGHTNESS;
        stepRange = team1AndOpponentTotal-1;
      }
      uint8_t critMod = (!hasHealed && attackTypeCounter == 2)? 2:1;
      stepTimer.set((scaledDuration)/critMod);
    }
    setColorOnFace(dim((!hasHealed && attackTypeCounter == 2)?RED:WHITE, attackStepBrightness), toCenterFace);
 }

  if (animCount>0){
    if (mainTimer.isExpired()){
      animCount-=1;
      uint8_t oppositeFace = OPPOSITE_FACE(startFace);
      if (animCount==2){
        setColorOnFace(YELLOW, startFace);
        mainTimer.set(ATTACK_ANIM_DURATION);
      }else if(animCount==1){
        setColorOnFace(YELLOW, oppositeFace);
        if (playerType == MUSHROOM){
          setColorOnFace(GREEN, startFace);
        }else{
          setColorOnFace(isActive?ORANGE:OFF, startFace);
        }
        mainTimer.set(ATTACK_ANIM_DURATION);
      }else{
        if (playerType == MUSHROOM){
          setColorOnFace(RED, oppositeFace);
          isAttacking=true;
          turnTimer.set(2000+(TURN_DURATION_PER_PLAYER*(startFace==awayFace?team1AndOpponentTotal:team2Total)));
        }else{
          setColorOnFace(isActive?WHITE:OFF, oppositeFace);
        }
        setValueSentOnFace(ACK_IDLE, startFace);
        setValueSentOnFace(holdData, oppositeFace);
      }
    }
  }

//**END Animations**

//**DATA parser**
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
    isAttacking=false;
    if (playerType != MUSHROOM){
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
  int pushFace = isFromCenter?awayFace:toCenterFace;
  int sameFace = isFromCenter?toCenterFace:awayFace;
  pulseTimer.set(0);
  if (playerType == MUSHROOM){
      if (headCount == 0){
        //handle win condition
        setColorOnFace(WHITE, pushFace);
        sendToggle(pushFace, 7, true);
      }else{
        setValueSentOnFace(ACK_IDLE, toCenterFace);
        setValueSentOnFace(ACK_IDLE, awayFace);
      }
    }else{
      if (aWinnerIsYou){
          holdData = (1<<2)+TOGGLE_TURN;
          startFace=sameFace;
          isWinning = hasWon = true;
          winAnimCount =0;
          setValueSentOnFace(ACK_IDLE, sameFace);
      }else{
        if (isFromCenter){
          //Using team1Total on non-mushroom to track enemy team size
          team1AndOpponentTotal = headCount;
          if(health>0){
            isActive = !isActive;
          }else{
            isActive = false;
          }
          refreshAllFaces();
          if (!isValueReceivedOnFaceExpired(awayFace)){
            sendToggle(awayFace, headCount, false);
            setValueSentOnFace(ACK_IDLE, sameFace);
          }else{
            sendToggle(toCenterFace, (health>0)?1:0, false);
          }
        }else{
          if(health>0){
              //calculate the numAlive
              headCount+=1;
          }
          sendToggle(toCenterFace, headCount, false);
          setValueSentOnFace(ACK_IDLE, sameFace);
       }
    }
  }
}

void sendToggle(int face, uint8_t aliveCount, bool aWinnerIsYou){
  uint8_t dataLoad = (aliveCount<<1)+(aWinnerIsYou?1:0);
  setValueSentOnFace((dataLoad<<2)+TOGGLE_TURN, face);
}

void handleSetupReset(boolean onWayOut, uint8_t blinkCount, int faceOfSignal){
  if (onWayOut){
    toCenterFace = faceOfSignal;
    awayFace = OPPOSITE_FACE(faceOfSignal);
  }
  if (playerType == MUSHROOM && !onWayOut){
    
    if (faceOfSignal == awayFace){
      team1AndOpponentTotal = blinkCount;
    }else if (faceOfSignal == toCenterFace){
      team2Total = blinkCount;
    }
    if (team2Total > 0 && team1AndOpponentTotal > 0){
        beginGame();
    }
  }else{
    setValueSentOnFace(ACK_IDLE, faceOfSignal);
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
  setColorOnFace((winAnimCount==0||winAnimCount==6)?GREEN:OFF, startFace);
  setColorOnFace((winAnimCount==1||winAnimCount==5)?GREEN:OFF, CW_FROM_FACE(startFace,1));
  setColorOnFace((winAnimCount==1||winAnimCount==5)?GREEN:OFF, CCW_FROM_FACE(startFace,1));
  setColorOnFace((winAnimCount==2||winAnimCount==4)?GREEN:OFF, CW_FROM_FACE(startFace,2));
  setColorOnFace((winAnimCount==2||winAnimCount==4)?GREEN:OFF, CCW_FROM_FACE(startFace,2));
  setColorOnFace(winAnimCount==3?GREEN:OFF, OPPOSITE_FACE(startFace));
}

void setUpAnim(){
  byte dimmer = (mainTimer.getRemaining()/10);
  byte dampenedDimmer = sin8_C(dimmer)/2;
  Color dimmedWhite = dim(WHITE, MAX_BRIGHTNESS - dampenedDimmer/2);
  setColor(dimmedWhite);
}

void youWon(){
  winAnimCount = 0;
  health = 4;
  setColor(GREEN);
  isWinning = false;
  //hasWon = true;
  winPassCount = 0;
}

void gameAnim(Color color){
  setColor(dim(color, MAX_BRIGHTNESS - mainTimer.getRemaining()));
}

void refreshSides(){
  if (isWinning)return;
  byte dimmer = (pulseTimer.getRemaining()/10);
  byte dampenedDimmer = sin8_C(dimmer)/2;
  Color slimeGreen = makeColorRGB(dampenedDimmer, 255, 0);
  Color slimeRed = dim(RED, MAX_BRIGHTNESS - (dampenedDimmer));
  setColorOnFace(health>0?slimeGreen:slimeRed, CW_FROM_FACE(awayFace,1));
  setColorOnFace(health>1?slimeGreen:slimeRed, CW_FROM_FACE(awayFace,2));
  setColorOnFace(health>2?slimeGreen:slimeRed, CCW_FROM_FACE(awayFace,1));
  setColorOnFace(health>3?slimeGreen:slimeRed, CCW_FROM_FACE(awayFace,2));
}

void refreshAllFaces(){
  isHit=false;
  refreshSides();
  setColorOnFace(isActive?ORANGE:OFF, awayFace);
  setColorOnFace(isActive?WHITE:OFF, toCenterFace);
}

void handleNewTurn(boolean isFromSetup){
  team1Turn = !team1Turn;
  if (team1Turn){
    if (!isFromSetup){
      setValueSentOnFace((team1AndOpponentTotal<<3)+TOGGLE_TURN, toCenterFace);
    }
    setValueSentOnFace((team2Total<<3)+TOGGLE_TURN, awayFace);
    setColorOnFace(GREEN, awayFace);
    setColorOnFace(RED, toCenterFace);
  }else{
    if (!isFromSetup){
      setValueSentOnFace((team2Total<<3)+TOGGLE_TURN, awayFace);
    }
    setValueSentOnFace((team1AndOpponentTotal<<3)+TOGGLE_TURN, toCenterFace);
    setColorOnFace(RED, awayFace);
    setColorOnFace(GREEN, toCenterFace);
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
  isActive= hasHealed = isAttacking= isHit= isError= isHealing= isWinning, hasWon = false;
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
  team1AndOpponentTotal = team2Total = 0;
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
