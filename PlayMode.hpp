#include "Mode.hpp"
#include "Scene.hpp"

#include "Connection.hpp"
#include "Game.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	void game_over();
	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;
	//Add player
	std::vector<Scene::Transform*> player_transforms;
	//
	void sync_players_to_scene();
	//connection to server:
	Client &client;
	Scene scene;
	//camera:
	Scene::Camera *camera = nullptr;
	Scene::Transform* playeryyq;
	size_t lastdrawable;
	//rotation
	float player_angle = 0.0f;
	float dash_cd = 0.0f;   
	bool can_dash = true;
};
