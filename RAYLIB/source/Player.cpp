#include "Player.h" // header in local directory
#include <iostream> // header in standard library

Player::Player() {
  Health = 100;
  isJumping = false;
  isCrouching = false;
  enum Movement movement = Movement::LEFT;
  enum Ability ability = Ability::IDLE;

}

void Player::Update(){
}

void Player::Draw() {

}

void Player::HandleMovement(enum Movement movement) {

}

void Player::HandleAbility(enum Ability ability) {

}

void Player::TakeDamage(int damage) {
  Health -= damage;
}