#ifndef BGA_CUSTOM_BLINKLIB
#error "This code requires a custom blinklib. See https://github.com/brunoga/blinklib/releases/latest"
#endif

#define NORMAL_ATTACK_DURATION 150
#define TURN_DURATION_PER_PLAYER 300
#define MUSHROOM_GRACE_PERIOD 2000
#define ATTACK_ANIM_DURATION 150
#define ANIM_DURATION_SHORT 20
#define ANIM_DURATION_MID 200
#define ANIM_DURATION_LONG 500
#define PULSE_TIMER_PER_HEALTH 637

byte faceOffsetArray[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };
#define CW_FROM_FACE(f, amt) faceOffsetArray[(f) + (amt)]
#define CCW_FROM_FACE(f, amt) faceOffsetArray[6 + (f) - (amt)]
#define OPPOSITE_FACE(f) CW_FROM_FACE((f), 3)

byte messageStoredForAway[] = {0,0,0,0,0,0};
byte messageStoredForCenter[] = {0,0,0,0,0,0};

/*Signals between blinks are either 
ACK_IDLE - Acknowledge and also allow identical signals to come in the same direction and be counted
SLUG_DATA - Slug attack data
TOGGLE_TURN - Changes turns and also checks for a win condition by counting dead pieces
SETUP_RESET - Initial setup or Resetting the game. Counts the team sizes.*/
enum dataMode{
  ACK_IDLE = 0,
  SLUG_DATA = 1,
  TOGGLE_TURN = 2,
  SETUP_RESET = 3
};

//pieces are either Mushrooms or Slug segments and everything starts as unassigned.
enum playerClass{
  MUSHROOM = 0,
  SLUG  = 1,
  UNASSIGNED = 2
};

//some of these run simulataneously (plus consolodating doesn't seem to help memory much)
Timer pulseTimer, turnTimer, mainTimer, stepTimer, gracePeriodTimer;

uint8_t playerType = UNASSIGNED;
uint8_t health = 4;
uint8_t awayFace, toCenterFace = FACE_COUNT;
uint8_t animCount = 0, attackTypeCounter;

//held data waiting to send (possibly after animation)
uint8_t startFace;
byte holdData;

byte team1AndOpponentTotal = 0, team2Total, stepRange = 0;
byte attackStepBrightness = MAX_BRIGHTNESS;
uint8_t hitAnimCount = 0;

uint8_t badFace = 6;
bool hasHealed;
bool isActive, isAttacking, isHit, isError, isHealing, isEndPiece = false; 
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
      //fire
      uint8_t range = stepRange;
      isAttacking = false;
      mainTimer.set(0);
      stepTimer.set(0);
      stepRange = team1AndOpponentTotal-1;
      boolean crit = attackTypeCounter == 2;
      uint8_t dataLoad = (range<<1)+(crit?1:0);
      queueMessage((dataLoad<<2)+SLUG_DATA, toCenterFace);
    }else{
      //begin sequence
      int scaledDuration = NORMAL_ATTACK_DURATION*health;
      if (!isValueReceivedOnFaceExpired(awayFace)){
        queueMessage((1<<2)+ACK_IDLE, awayFace);
      }
      queueMessage((1<<2)+ACK_IDLE, toCenterFace);
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


  if (gracePeriodTimer.isExpired()){
    if (isEndPiece){
      if (isValueReceivedOnFaceExpired(toCenterFace)){
        showErrorOnFace(toCenterFace);
      }
    }else{
      if (isValueReceivedOnFaceExpired(awayFace)){
        showErrorOnFace(awayFace);
      }
      if (isValueReceivedOnFaceExpired(toCenterFace)){
        showErrorOnFace(toCenterFace);
      }
    }
    if (playerType==MUSHROOM && (team1AndOpponentTotal == 0 || team2Total == 0)){
      setUpGame();
    }
  }
  
  //state logic
  if (isHit){
    if(mainTimer.isExpired()){
      mainTimer.set(ANIM_DURATION_SHORT*5);
      if (hitAnimCount>(((holdData&4)==4)?18:6)){
        isHit = false;
        hitAnimCount=0;
        refreshAllFaces();
      }else{
        explosionAnim();
        hitAnimCount++;
      }
    }
    
  }else if (isError){
    if(mainTimer.isExpired()){
      mainTimer.set(ANIM_DURATION_MID);
      attackTypeCounter -=1;
    }
    if (badFace < 6){
      faceAnim(RED,badFace);
    }else{
      gameAnim(RED);
    }
    if (attackTypeCounter ==0){
      badFace = 6;
        isError= false;
        refreshAllFaces();
    }
  }else if (isHealing){
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

 }else if (isAttacking){
    if (turnTimer.isExpired() && playerType == MUSHROOM){
        //toggleTurns
        handleNewTurn(false);
        isAttacking = false;
    }

    //attack sequence animation
    if(playerType != MUSHROOM) {
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
      setColorOnFace(dim((!hasHealed && attackTypeCounter == 2)?RED:WHITE, attackStepBrightness), CW_FROM_FACE(toCenterFace,1));
      setColorOnFace(dim((!hasHealed && attackTypeCounter == 2)?RED:WHITE, attackStepBrightness), CCW_FROM_FACE(toCenterFace,1));
   }
 }

  //pulse while waiting initial setup
  if (playerType == UNASSIGNED){
    if (mainTimer.isExpired()){
       mainTimer.set(2550);
    }
    setUpAnim();
  }

  //slug body pulsing/breathing
  if (pulseTimer.isExpired()){
    if (playerType == SLUG && health >0){
      pulseTimer.set(PULSE_TIMER_PER_HEALTH*health);
    }
  }else{
    if (playerType == SLUG && !( isHit || isError || isHealing )){//|| hasWonisWinning ||
      refreshSides();
    }
  }

  //path of the cannon round
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
          turnTimer.set( ( ((holdData&4)==4)?1800:600 ) + (TURN_DURATION_PER_PLAYER*( (startFace==awayFace)?(team1AndOpponentTotal+1):(team2Total+1) ) ) );
        }else{
          setColorOnFace(isActive?WHITE:OFF, oppositeFace);
        }
        queueMessage(ACK_IDLE, startFace);
        queueMessage(holdData, oppositeFace);
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
  processQueue();
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
          queueMessage((1<<2)+ACK_IDLE, awayFace);
        }
      }else{
        queueMessage((1<<2)+ACK_IDLE, toCenterFace);
      }
    }
  }
  queueMessage(ACK_IDLE, isFromCenter?toCenterFace:awayFace);
}

void handleToggle(uint8_t headCount, boolean aWinnerIsYou, boolean isFromCenter){
  int pushFace = isFromCenter?awayFace:toCenterFace;
  int sameFace = isFromCenter?toCenterFace:awayFace;
  pulseTimer.set(0);
  if (playerType == MUSHROOM){
      if (headCount == 0){
        //handle win condition
        //replace this with a mushroom anim?
        setColorOnFace(WHITE, pushFace);
        queueMessage(ACK_IDLE, pushFace);
        //sendToggle(pushFace, 7, true);
      }else{
        queueMessage(ACK_IDLE, toCenterFace);
        queueMessage(ACK_IDLE, awayFace);
      }
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
            queueMessage(ACK_IDLE, sameFace);
          }else{
            sendToggle(toCenterFace, (health>0)?1:0, false);
          }
        }else{
          if(health>0){
              //calculate the numAlive
              headCount+=1;
          }
          sendToggle(toCenterFace, headCount, false);
          queueMessage(ACK_IDLE, sameFace);
       }
    }
}

void sendToggle(int face, uint8_t aliveCount, bool aWinnerIsYou){
  uint8_t dataLoad = (aliveCount<<1)+(aWinnerIsYou?1:0);
  queueMessage((dataLoad<<2)+TOGGLE_TURN, face);
}

void handleSetupReset(boolean onWayOut, uint8_t blinkCount, int faceOfSignal){
  if (onWayOut){
    toCenterFace = faceOfSignal;
    awayFace = OPPOSITE_FACE(faceOfSignal);
    gracePeriodTimer.set(MUSHROOM_GRACE_PERIOD);
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
    queueMessage(ACK_IDLE, faceOfSignal);
    if (onWayOut){
      //winPosition = blinkCount;
      setPlayerType(SLUG);
      if (!isValueReceivedOnFaceExpired(awayFace)){
        uint8_t dataLoad = ((blinkCount+1)<<1)+1;
        queueMessage((dataLoad<<2)+SETUP_RESET, awayFace);
      }else{
        //pass it back
        isEndPiece = true;
        queueMessage((blinkCount<<3)+SETUP_RESET, toCenterFace);
      }
    }else{
      queueMessage((blinkCount<<3)+SETUP_RESET, toCenterFace);
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

void queueMessage(byte message, int faceIndex){
  if (faceIndex == awayFace){
    if (messageStoredForAway[0]==0){
      messageStoredForAway[0]=1;
      messageStoredForAway[1]=message;
    }else if (messageStoredForAway[2]==0){
      messageStoredForAway[2]=1;
      messageStoredForAway[3]=message;
    }else if (messageStoredForAway[4]==0){
      messageStoredForAway[4]=1;
      messageStoredForAway[5]=message;
    }
  }else if (faceIndex == toCenterFace){
    if (messageStoredForCenter[0]==0){
      messageStoredForCenter[0]=1;
      messageStoredForCenter[1]=message;
    }else if (messageStoredForCenter[2]==0){
      messageStoredForCenter[2]=1;
      messageStoredForCenter[3]=message;
    }else if (messageStoredForCenter[4]==0){
      messageStoredForCenter[4]=1;
      messageStoredForCenter[5]=message;
    }
  }
}

void processQueue(){
  if (messageStoredForCenter[0]==1){
    byte message = messageStoredForCenter[1];
    messageStoredForCenter[0]=messageStoredForCenter[2];
    messageStoredForCenter[1]=messageStoredForCenter[3];
    messageStoredForCenter[2]=messageStoredForCenter[4];
    messageStoredForCenter[3]=messageStoredForCenter[5];
    messageStoredForCenter[4]=0;
    messageStoredForCenter[5]=0;
    setValueSentOnFace(message,toCenterFace);
  }
  if (messageStoredForAway[0]==1){
    byte message = messageStoredForAway[1];
    messageStoredForAway[0]=messageStoredForAway[2];
    messageStoredForAway[1]=messageStoredForAway[3];
    messageStoredForAway[2]=messageStoredForAway[4];
    messageStoredForAway[3]=messageStoredForAway[5];
    messageStoredForAway[4]=0;
    messageStoredForAway[5]=0;
    setValueSentOnFace(message,awayFace);
  }
}

void explosionAnim(){
  setColorOnFace(hitAnimColor(0), startFace);
  setColorOnFace(hitAnimColor(1), CW_FROM_FACE(startFace,1));
  setColorOnFace(hitAnimColor(1), CCW_FROM_FACE(startFace,1));
  setColorOnFace(hitAnimColor(2), CW_FROM_FACE(startFace,2));
  setColorOnFace(hitAnimColor(2), CCW_FROM_FACE(startFace,2));
  setColorOnFace(hitAnimColor(3), OPPOSITE_FACE(startFace));
}

Color hitAnimColor(int tierGroup){
  switch(hitAnimCount){
    case 0: return WHITE;
    case 13:
    case 1: return (tierGroup == 0)?WHITE:OFF;
    case 12:
    case 14:
    case 2: if (tierGroup == 0){
        return YELLOW;
      }else if (tierGroup == 1){
        return WHITE;
      }
      return OFF;
    case 11:
    case 15:
    case 3: if (tierGroup == 0){
        return ORANGE;
      }else if (tierGroup == 1){
        return YELLOW;
      }else if (tierGroup == 2){
        return WHITE;
      }
      return OFF;
    case 10:
    case 16:
    case 4: if (tierGroup == 0){
        return OFF;
      }else if (tierGroup == 1){
        return ORANGE;
      }else if (tierGroup == 2){
        return YELLOW;
      }
      return WHITE;
    case 9:
    case 17:
    case 5: if (tierGroup == 2){
        return ORANGE;
      }else if (tierGroup == 3){
        return YELLOW;
      }
      return OFF;
     case 6: 
     case 18:
     case 8: return (tierGroup == 3)?ORANGE:OFF;
     case 7: 
    default: return OFF;
  }
}

void setUpAnim(){
  byte dimmer = (mainTimer.getRemaining()/10);
  byte dampenedDimmer = sin8_C(dimmer)/2;
  Color dimmedWhite = dim(WHITE, MAX_BRIGHTNESS - dampenedDimmer/2);
  setColor(dimmedWhite);
}

void gameAnim(Color color){
  setColor(dim(color, MAX_BRIGHTNESS - mainTimer.getRemaining()));
}

void faceAnim(Color color, int face){
  setColorOnFace(dim(color, MAX_BRIGHTNESS - mainTimer.getRemaining()),face);
}

void refreshSides(){
  if (isAttacking)return;//isWinning||
  if (playerType == MUSHROOM){
    setColorOnFace(WHITE, CW_FROM_FACE(awayFace,1));
    setColorOnFace(WHITE, CW_FROM_FACE(awayFace,2));
    setColorOnFace(WHITE, CCW_FROM_FACE(awayFace,1));
    setColorOnFace(WHITE, CCW_FROM_FACE(awayFace,2));
    setColorOnFace(GREEN, team1Turn?awayFace:toCenterFace);
    setColorOnFace(RED, team1Turn?toCenterFace:awayFace);
    return;
  }
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
  if(playerType==SLUG){
    setColorOnFace(isActive?ORANGE:OFF, awayFace);
    setColorOnFace(isActive?WHITE:OFF, toCenterFace);
  }
}

void handleNewTurn(boolean isFromSetup){
  team1Turn = !team1Turn;
  if (team1Turn){
    if (!isFromSetup){
      queueMessage((team1AndOpponentTotal<<3)+TOGGLE_TURN, toCenterFace);
    }
    queueMessage((team2Total<<3)+TOGGLE_TURN, awayFace);
    setColorOnFace(GREEN, awayFace);
    setColorOnFace(RED, toCenterFace);
  }else{
    if (!isFromSetup){
      queueMessage((team2Total<<3)+TOGGLE_TURN, awayFace);
    }
    queueMessage((team1AndOpponentTotal<<3)+TOGGLE_TURN, toCenterFace);
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
  //winPassCount = 0;
  isActive= hasHealed = isAttacking= isHit= isError= isHealing = isEndPiece = false; //= isWinning = hasWon
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
  gracePeriodTimer.set(MUSHROOM_GRACE_PERIOD);
  FOREACH_FACE(face){
    //check if end
    if (!isValueReceivedOnFaceExpired(face)){
      if (toCenterFace == FACE_COUNT){
        toCenterFace = face;
        queueMessage(ACK_IDLE,face);
      }else{
        awayFace = face;
        queueMessage(ACK_IDLE,face);
      }
      queueMessage((3<<2)+SETUP_RESET,face);
    }
  }
}

 void showError(){
    isError = true;
    attackTypeCounter = 3;
 }

void showErrorOnFace(int errorFace){
   isError = true;
   badFace = errorFace;
   attackTypeCounter = 3;
}

 void healUp(){
    isHealing = true;
    attackTypeCounter = 3;
 }
 //***END Utils***
