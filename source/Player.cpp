#include "Player.h" // header in local directory
#include <iostream> // header in standard library

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

int Player::getHealth() {
  return Health;
}

bool Player::getJumping() {
  return isJumping;
}

bool Player::getCrouching() {
  return isCrouching;
}