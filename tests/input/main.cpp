#include <stdio.h>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <thread>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <SDL2/SDL.h>

#include "messages.hpp"
#include "cluon-complete.hpp"
/*
message opendlv.proxy.GroundSteeringRequest [id = 1090] {
  float groundSteering [id = 1];
}

message opendlv.proxy.GroundAccelerationRequest [id = 1092] {
  float groundAcceleration [id = 1];
}

message opendlv.proxy.GroundDecelerationRequest [id = 1093] {
  float groundDeceleration [id = 1];
}




*/



int main(int argc, char * argv[]) {

  cluon::OD4Session od4(111,[](cluon::data::Envelope &&envelope) {
    std::cout << "SENT " << envelope.dataType()<< std::endl;
  });
  if(od4.isRunning()){
    std::cout << "OD4Session running" << std::endl;
    std::cout << "Waiting for commands" << std::endl;
  }else{
    std::cout << "OD4Session is not running" << std::endl;
  }
  bool quit = false;
  SDL_Event event;
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window * window = SDL_CreateWindow("VDRIFT INPUT",SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  opendlv::proxy::GroundSteeringRequest steer;
  opendlv::proxy::GroundAccelerationRequest acc;
  opendlv::proxy::GroundDecelerationRequest dec;
  while (od4.isRunning() && !quit){
    if(!SDL_PollEvent(&event)) continue;
    switch (event.type){
      case SDL_QUIT:
        quit = true;
        break;
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym){
            case SDLK_q:
              quit = true;
              break;
            case SDLK_LEFT:
              if(steer.groundSteering() < 0.0) break;
              std::cout << "KEY DOWN SDLK_LEFT" << std::endl;
              steer.groundSteering(-1.0);
              od4.send(steer);
              break;
            case SDLK_RIGHT:
              if(steer.groundSteering() > 0.0) break;
              std::cout << "KEY DOWN SDLK_RIGHT" << std::endl;
              steer.groundSteering(1.0);
              od4.send(steer);
              break;
            case SDLK_UP:
              if(acc.groundAcceleration() > 0.0) break;
              std::cout << "KEY DOWN SDLK_UP" << std::endl;
              acc.groundAcceleration(1.0);
              od4.send(acc);
              break;
            case SDLK_DOWN:
              if(dec.groundDeceleration() > 0.0) break;
              std::cout << "KEY DOWN SDLK_DOWN" << std::endl;
              dec.groundDeceleration(1.0);
              od4.send(dec);
              break;
        }
        break;
      case SDL_KEYUP:
        switch (event.key.keysym.sym){
            case SDLK_q:
              quit = true;
              break;
            case SDLK_LEFT:
            case SDLK_RIGHT:
              std::cout << "KEY UP SDLK_LEFT|SDLK_RIGHT" << std::endl;
              steer.groundSteering(0.0);
              od4.send(steer);
              break;
            case SDLK_UP:
              std::cout << "KEY UP SDLK_UP" << std::endl;
              acc.groundAcceleration(0.0);
              od4.send(acc);
              break;
            case SDLK_DOWN:
              std::cout << "KEY UP SDLK_DOWN" << std::endl;
              dec.groundDeceleration(0.0);
              od4.send(dec);
              break;
        }
        break;
    }

    //SDL_RenderClear(renderer);
    //SDL_RenderPresent(renderer);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::cout << "EXITING" << std::endl;
	return 0;
}