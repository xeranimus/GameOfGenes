#include "Controller.h"
#include <exception>
#include <sstream>
#include <stdexcept>

bool endsWith(std::string& str, std::string& suffix)
{
	return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

template <typename TYPE>
std::vector<TYPE> arrayToVector(TYPE x[], int size)
{
	std::vector<TYPE> vect;
	for (int i = 0; i < size; i++)
	{
		vect.push_back(x[i]);
	}
	return vect;
}

Controller::Controller(SDL_Window * window)
{
	std::cerr << "controller constructor called\n";
	if (window == NULL)
	{
		std::cerr << "WINDOW IS NULL";
		throw "Window failed to create.";
	}
	this->mainRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if( mainRenderer == NULL )
	{
		std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError() << std::endl;
		throw "Renderer failed to create.";
	}
    //Set default speed to 25
    speed = 25;

	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	std::cerr << windowWidth << " height: " << windowHeight << std::endl;

	//TODO: add options to make this mutable
	this->boardPanel = {0, 0, windowWidth, windowHeight * 5 / 6};

	std::cerr << "Board panel: " << boardPanel.w << " " << boardPanel.h << std::endl;

	this->statusPanel = {0,  windowHeight * 5 / 6, windowWidth, windowHeight / 6};

	std::cerr << "Status panel: " << this->statusPanel.w << " " << this->statusPanel.h << std::endl;

	this->boardPosition = {0, 0};
	/*
	int gcf = greatestCommonFactor(this->boardPanel.w, this->boardPanel.h);
	xStep = this->boardPanel.w / gcf;
	yStep = this->boardPanel.h / gcf;
	*/

	this->mainColor = {0, 0xFF, 0};
	this->bgColor = {0, 0, 0};
	this->accentColor = {0xFF, 0xFF, 0xFF};
    state = MENU;

	//TODO: move this into the controller
	try
	{
		this->mainFont = TTF_OpenFont("./font/UbuntuMono-Bold.ttf", 16);
	}
	catch (const char** ex)
	{
		std::cerr << ex << std::endl;
		std::cerr << "Error. Font cannot be found." << std::endl;
	}
}

Controller::~Controller()
{
	TTF_CloseFont(mainFont);
	if (board != nullptr)
	{
		delete board;
		board = nullptr;
	}
	SDL_DestroyRenderer(mainRenderer);
}

void Controller::createNewBoard(bool wrapAround, int height, int width)
{
    //delete the old board if necessary
	std::cerr << "creating new board\n";
    if (board != nullptr)
    {
        delete board;
        board = nullptr;
    }
	board = new Board(wrapAround, height, width);

	//TODO: handle crazy large boards
	resetZoom();
	clearScreen();
	renderStatusPanel();
	renderBoard();
    updateScreen();
}

void Controller::createNewBoard(std::string filename)
{
    //delete previous board if necessary
    if(board != NULL){
        delete board;
        board = NULL;
    }
    //May throw an error if the file does not exist
    board = new Board("saved" + separator() + filename);
	resetZoom();
	clearScreen();
	renderStatusPanel();
	renderBoard();
	updateScreen();
}

void Controller::createNewBoard(bool wrapAround)
{
	//calculates the maximum possible board size
	//TODO: make fill settings
	int maxWidth = boardPanel.w / 3;
	int maxHeight = boardPanel.h / 3;
	createNewBoard(wrapAround, maxWidth, maxHeight);
}

void Controller::randomizeBoard(double ratio)
{
    board->randomize(ratio);
}

void Controller::runIteration()
{
    board->runIteration();
}

int Controller::getSpeed()
{
    return speed;
}

controlState Controller::getState()
{
    return state;
}

std::string Controller::getStateName()
{
    switch(state)
    {
        case RUNNING:
            return "Running";
        case PAUSED:
            return "Paused";
		case PLACE:
			return "Place";
        case EDITING:
            return "Editing";
        case MENU:
            return "Menu";
        case EXITING:
            return "Exiting";
    }
	//if for some reason the state wasn't found, assume its in the menu
    return "MENU";
}

int Controller::getButtonInput(std::string dialog, std::vector<std::string> options)
{
	std::cerr << "get button input\n";
	ButtonBox buttons(mainRenderer, mainFont, mainColor, bgColor, dialog, options, boardPanel.w / 2, boardPanel.h / 2, true);

	std::cerr << "buttons created\n";
	buttons.render(mainRenderer);
	std::cerr << "buttons rendered\n";
	updateScreen();
	std::cerr << "render presented\n";

	bool updateRender = false;
	while (!buttons.hasValidInput())
	{
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT)
			{
				exit(0); //TODO: exit more gracefully
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				int a, b;
				SDL_GetMouseState(&a, &b);
				buttons.updateInput(a, b);
				updateRender = true;
			}
			else if (event.type == SDL_MOUSEMOTION)
			{
				int a, b;
				SDL_GetMouseState(&a, &b);
				buttons.updateHover(a, b);
				updateRender = true;
			}
		}
		if (updateRender)
		{

			buttons.render(mainRenderer);
			updateScreen();
			updateRender = false;
		}
	}
	return buttons.getInput();
}

bool Controller::getYesOrNo(std::string dialog)
{
	std::vector<std::string> options;
	options.push_back("Yes");
	options.push_back("No");
	//the exclamation point is because Yes corresponds to 0, which is false
	//intuitively, yes should correspond to "true";
	return !getButtonInput(dialog, options);
}

void Controller::getConfirmationBox(std::string dialog)
{
	std::vector<std::string> options;
	options.push_back("Ok.");
	getButtonInput(dialog, options);
}

//To any friends, family, or future employers, I apologize for this pure spaghetti
void Controller::getKeybindingsBox()
{

	if (generalControls == nullptr)
	{
		std::string generalText =
			"GENERAL CONTROLS"
			"\n+, =\tZoom in"
			"\n-\tZoom out"
			"\nArrow Keys\tMove Cursor / Pan Camera"
			"\nScrollwheel \tZoom in / Out"
			"\nRight-Click\tPan Camera"
			"\nR\tReset Zoom"
			"\nH\tShow Help Menu";
		generalControls = new GridBox(mainRenderer, mainFont, generalText, mainColor, bgColor);
	}
	if (pressAnyKey == nullptr)
	{
		std::string pressAnyText = "Press any key to continue.";
		pressAnyKey = new GridBox(mainRenderer, mainFont, pressAnyText, mainColor, bgColor);
	}
	switch (state)
	{
		case PAUSED:
			if (pausedControls == nullptr)
			{
				std::string pausedText =
					"PAUSED MODE"
					"\nSpacebar\tToggle Cell"
					"\nLeft-Click\tToggle Cell"
					"\nEnter/Return\tPerform One Iteration"
					"\n] (Right Bracket)\tIncrease Speed"
					"\n[ (Left Bracket)\tDecrease Speed"
					"\nLeft-Click\tToggle Cell"
					"\nP\tPlay (enter running mode)"
					"\nA\tPlace pattern"
					"\nESC\tMain Menu";
				pausedControls = new GridBox(mainRenderer, mainFont, pausedText, mainColor, bgColor);
			}
			pausedControls->render(mainRenderer, 0, 0, false, LEFT);
			break;
		case RUNNING:
			if (runningControls == nullptr)
			{
				std::string runningText =
					"RUNNING MODE"
					"\n] (Right Bracket)\tIncrease Speed"
					"\n[ (Left Bracket)\tDecrease Speed"
					"\nP\tPause (enter paused mode)"
					"\nESC\tExit to Menu";
				runningControls = new GridBox(mainRenderer, mainFont, runningText, mainColor, bgColor);
			}
			runningControls->render(mainRenderer, 0, 0, false, LEFT);
			break;
		case PLACE:
			if (placeControls == nullptr)
			{
				std::string placeText =
					"PLACE MODE"
					"\nSpacebar\tPlace Pattern"
					"\nLeft-Click\tPlace Pattern"
					"\nEnter/Return\tPlace Pattern"
					"\n] (Right Bracket)\tRotate Right"
					"\n[ (Left Bracket)\tRotate Left"
					"\nA, ESC\tExit Place Mode";
				placeControls = new GridBox(mainRenderer, mainFont, placeText, mainColor, bgColor);
			}
			placeControls->render(mainRenderer, 0, 0, false, LEFT);
			break;
		case EDITING:
			if (editControls == nullptr)
			{
				std::string editText =
					"EDIT MODE"
					"\nSpacebar\tToggle Cell"
					"\nLeft-Click\tToggle Cell"
					"\nA\tPlace pattern"
					"\nESC\tMain Menu";
				editControls = new GridBox(mainRenderer, mainFont, editText, mainColor, bgColor);
			}
			editControls->render(mainRenderer, 0, 0, false, LEFT);
			break;
	}


	std::cerr << "Dimensions: " << generalControls->getWidth() << " , " << generalControls->getHeight() << std::endl;
	generalControls->render(mainRenderer, boardPanel.w - generalControls->getWidth() , 0, false, LEFT);
	pressAnyKey->render(mainRenderer, boardPanel.w - generalControls->getWidth() / 2 - pressAnyKey->getWidth() / 2 , generalControls->getHeight(), false, CENTER);
	//TODO: possibly make a confirmation box?
	SDL_RenderPresent(mainRenderer);
	bool quit = false;
	while (!quit)
	{
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT)
			{
				exit(0); //TODO: exit more gracefully
			}
			else if (event.type == SDL_KEYDOWN)
				quit = true;
		}
	}
	std::cerr << "Help box coming soon." << std::endl;
}

std::string Controller::getStringInput(std::string message)
{
	TextBox inputBox(mainRenderer, mainFont, mainColor, bgColor, accentColor, message, boardPanel.w / 2, boardPanel.h / 2, true);

	SDL_Event * e = new SDL_Event();
	bool quit = false;
	bool updateRender = true;

	SDL_StartTextInput();
	while (!quit)
	{
		while (SDL_PollEvent(e) != 0)
		{
			if (e->type == SDL_QUIT)
			{
				quit = true;
				//TODO: exit more gracefully
				exit(-1);
			}
			else if (e->type == SDL_KEYDOWN)
			{
				if (e->key.keysym.sym == SDLK_BACKSPACE)
				{
					inputBox.backspace();
					updateRender = true;
				}
				updateRender = true;
				if (e->key.keysym.sym == SDLK_RETURN)
				{
					quit = true;
				}
				//TODO: allow copy and paste
			}
			else if (e->type == SDL_TEXTINPUT)
			{
				//TODO: handle copy and paste
				inputBox.append(e->text.text);
			}
		}
		if (updateRender)
		{
			inputBox.render(mainRenderer);
			updateScreen();
			updateRender = false;
		}
	}
	SDL_StopTextInput();
	return inputBox.getInput();
}

double Controller::getRatioInput(std::string message)
{
	double result = 0;
	bool invalidInput = true;
	while (invalidInput)
	{
		std::string input  = getStringInput(message);

		if (input.size() == 0)
		{
			getConfirmationBox("Error. Input must be only digits, \'.\', and \'-\'.");
			clearScreen();
			continue;
		}

		//TODO: possibly revise this
		std::string::iterator iter = input.begin();
		while (iter != input.end() && (std::isdigit(*iter) || *iter == '.' || *iter == '-') )
			iter++;
		if (iter != input.end())
		{
			continue;
		}


		invalidInput = false;
		try
		{
			result = std::stod(input);
		}
		catch (const std::invalid_argument& ex)
		{
			getConfirmationBox(ex.what());
			clearScreen();
			invalidInput = true;
			continue;
		}
	}

    return result;
}

int Controller::getIntInput(std::string message)
{
	int result = 0;
	bool invalidInput = true;
	while (invalidInput)
	{
		std::string input  = getStringInput(message);

		if (input.size() == 0)
		{
			continue;
		}

		//TODO: possibly revise this
		std::string::iterator iter = input.begin();
		while (iter != input.end() && std::isdigit(*iter))
			iter++;
		if (iter != input.end())
		{
			getConfirmationBox("Error. Input must be an positive integer value.");
			clearScreen();
			continue;
		}


		invalidInput = false;
		try
		{
			result = std::stoi(input);
		}
		catch (const std::invalid_argument& ex)
		{
			getConfirmationBox(ex.what());
			clearScreen();
			invalidInput = true;
			continue;
		}
	}

	return result;
}

void Controller::saveCurrent()
{
	bool shouldSave = getYesOrNo("Would you like to save?");
	//clearScreen();
	renderBoard();
	renderStatusPanel();
    if(shouldSave)
    {
        std::string filename = getStringInput("Enter a filename:");
        //If the state is editing, then the program is in the pattern editor
        //so the file should be saved to the pattern folder
        if(state == EDITING)
        {
            filename = "saved" + separator() + filename;
        }
        //Otherwise, save it to the board folder
        else
        {
            filename = "saved" + separator() + filename;
        }
		if (!endsWith(filename, ".brd"))
			filename += ".brd";
        board->saveState(filename);
    }
}

void Controller::setState(controlState newState)
{
    state = newState;
	//TODO: delete if this doesn't affect anything:
    //if(state != EXITING)
    //    renderStatusPanel();
}

void Controller::setSpeed(int newSpeed)
{
    /*adds the value to the current value of speed. Prevents users from
    decreasing below 1, and exceeding 10000
    scaling the increment depending on the size of the speed (to allow users
    to scroll at a reasonable rate) */
    if (speed >= 10000)
    {
        newSpeed *= 1000;
    }

    else if (speed >= 10000) {
        newSpeed *= 1000;
    }
    else if (speed >= 1000)
    {
        newSpeed *= 100;
    }
    else if (speed >= 100)
    {
        newSpeed *= 10;
    }
    //make sure the speed is >= 1
    if (speed+newSpeed < 1)
    {
        speed = 1;
        return;
    }
    //make sure speed does not exceed 100000
    if (speed+newSpeed > 100000)
    {
        speed = 100000;
        return;
    }
    speed += newSpeed;
}



void Controller::setPan(int x, int y)
{
	boardPosition.x -= (x - boardPanel.w / 2) * 10 / boardPanel.w * cellWidth / 40;
	boardPosition.y -= (y - boardPanel.h / 2) * 10 / boardPanel.h * cellHeight / 40;
	/*
	if (x * 4 / boardPanel.w < (boardPanel.w / 2 / cellWidth))
	{
		boardPosition.x += cellWidth / 2;
	}
	else
	{
		boardPosition.x -= cellWidth / 2;
	}*/
	checkRC();
}

void Controller::setZoom(int amount)
{
	//put it all in a for loop
	//check that the cellWidth > 0
	//TODO: check cell height as well?
	if (cellWidth + amount <= 2)
	{
		return;
	}
	//TODO: zoom if one of the dimensions can fit inside
	//maybe even zoom in that direction?



	//TODO: for loops suck, use a series instead
	for (int i = 0; i < amount; i++)
	{
		//std::cout << (boardPanel.w - cellWidth + 1) << " " << (currentCol * cellWidth + boardPosition.x + 1) + 1 << std::endl;
		//std::cout << (boardPanel.w - cellWidth + 1) / (currentCol * cellWidth + boardPosition.x + 1) + 1 << std::endl;
		//int freq = (boardPanel.w - cellWidth + 1) / (currentCol * cellWidth + boardPosition.x + 1) + 1;
		//boardPosition.x -= currentCol + ((cellWidth % board->getWidth() / (currentCol+1) == 0) ? 1 : 0);
		//boardPosition.y -= currentRow + ((cellWidth % board->getHeight() / (currentRow+1) == 0) ? 1 : 0);
		boardPosition.x -= currentCol + (cellWidth % 2);
		boardPosition.y -= currentRow + (cellWidth % 2);
		cellWidth++;
		cellHeight++;
	}
	for (int i = 0; i > amount; i--)
	{
		cellWidth--;
		cellHeight--;
		boardPosition.x += currentCol + (cellWidth % 2);
		boardPosition.y += currentRow + (cellHeight % 2);
	}

	if (board->getWidth() * cellWidth <= boardPanel.w)
	{
		boardPosition.x = (boardPanel.w - cellWidth * board->getWidth()) / 2;
	}
	else
	{
		if (boardPosition.x < 0)
		{
			if (board->getWidth() * cellWidth + boardPosition.x < boardPanel.x + boardPanel.w)
			{
				boardPosition.x = boardPanel.x + boardPanel.w - cellWidth * board->getWidth();
			}
		}
		else if (boardPosition.x > 0)
		{
			if (boardPosition.x > boardPanel.x)
			{
				boardPosition.x = boardPanel.x;
			}
		}
	}
	if (board->getHeight() * cellHeight <= boardPanel.h)
	{
		boardPosition.y = (boardPanel.h - cellHeight * board->getHeight())/2;
	}
	else
	{
		if (boardPosition.y < 0)
		{
			if (board->getHeight() * cellHeight + boardPosition.y < boardPanel.y + boardPanel.h)
			{
				boardPosition.y = boardPanel.y + boardPanel.h - cellHeight * board->getHeight();
			}
		}
		else if (boardPosition.y > 0)
		{
			if (boardPosition.y > boardPanel.y)
			{
				boardPosition.y = boardPanel.y;
			}
		}
	}
	checkRC();

	/*
	boardPosition.x -= (amount * (currentCol * cellWidth + boardPosition.x));
	boardPosition.y -= (amount * (currentRow * cellHeight + boardPosition.y));

	cellWidth += amount;
	cellHeight += amount;
	*/
	//amount is substracted, because generally positive zoom = more zoomed in
	//thus a positive zoom means we must remove cells

}

void Controller::resetZoom()
{
	cellWidth = boardPanel.w / this->board->getWidth();
	cellHeight = boardPanel.h / this->board->getHeight();
	if (cellHeight < 3)
		cellHeight = 3;
	if (cellWidth < 3)
		cellWidth = 3;

	if (cellWidth > cellHeight)
	{
		cellWidth = cellHeight;
	}
	else
	{
		cellHeight = cellWidth;
	}
	this->boardPosition.x = 0;
	this->boardPosition.y = 0;
	setZoom(0);
}


void Controller::updateRC(int x, int y)
{
	this->currentCol = (x - boardPanel.x - boardPosition.x) / cellWidth;
	this->currentRow = (y - boardPanel.y - boardPosition.y) / cellHeight;
	if (currentRow < 0)
		currentRow = 0;
	else if (currentRow > board->getHeight() - 1)
		currentRow = board->getHeight() - 1;
	if (currentCol < 0)
		currentCol = 0;
	else if (currentCol > board->getWidth() - 1)
		currentCol = board->getWidth() - 1;
}

void Controller::checkRC()
{
	//checking that currentRow and currentCol are in possible ranges
	if (currentRow < 0)
		currentRow = 0;
	else if (currentRow > board->getHeight() - 1)
		currentRow = board->getHeight() - 1;
	if (currentCol < 0)
		currentCol = 0;
	else if (currentCol > board->getWidth() - 1)
		currentCol = board->getWidth() - 1;

	//adjusts camera if row or col is out of range
	if (currentRow * cellHeight + boardPosition.y < boardPanel.y)
	{
		boardPosition.y = (boardPanel.y - (currentRow * cellHeight));
	}
	else if ((currentRow + 1) * cellHeight + boardPosition.y > (boardPanel.y + boardPanel.h))
	{
		boardPosition.y = (boardPanel.y + boardPanel.h ) - (currentRow + 1) * cellHeight;
	}
	if (currentCol * cellWidth + boardPosition.x < boardPanel.x)
	{
		boardPosition.x = (boardPanel.x - (currentCol * cellWidth));
	}
	else if ((currentCol + 1) * cellWidth + boardPosition.x > (boardPanel.x + boardPanel.w))
	{
		boardPosition.x = (boardPanel.x + boardPanel.w ) - (currentCol + 1) * cellWidth;
	}
}

void Controller::updateScreen()
{
	SDL_RenderPresent(mainRenderer);
}

void Controller::clearScreen()
{
	SDL_RenderClear(mainRenderer);
}

//still slow, likely due to the massive sizes
void Controller::renderBoard(SDL_Rect * renderArea)
{
	std::cerr << "render board called\n";
	std::vector<std::vector<bool>> matrix = board->getMatrix();

	//adjust the boundaries?
	int minRow = (renderArea->y - boardPosition.y) / cellHeight;
	int maxRow = (renderArea->y + renderArea->h - boardPosition.y) / cellHeight + 1;
	int minCol = (renderArea->x - boardPosition.x) / cellWidth;
	int maxCol = (renderArea->x + renderArea->w - boardPosition.x) / cellWidth + 1;

	minRow = (minRow > 0) ? minRow : 0;
	maxRow = (maxRow < board->getHeight()) ? maxRow : board->getHeight();
	minCol = (minCol > 0) ? minCol : 0;
	maxCol = (maxCol < board->getWidth()) ? maxCol : board->getWidth();
	//changing the color
	std::cerr << minRow << "," << maxRow << ";" << minCol << "," << maxCol << std::endl;
	SDL_SetRenderDrawColor(mainRenderer, mainColor.r, mainColor.g, mainColor.b, mainColor.a);
	int cellCount = 0, totalCount = 0;
	for (int row = minRow; row < maxRow; row++)
	{
		for (int column = minCol; column < maxCol; column++)
		{
			int y = renderArea->y + cellHeight * row + boardPosition.y;
			int x = renderArea->x + cellWidth * column + boardPosition.x;
			SDL_Rect cellRect = {x, y, cellWidth, cellHeight};

			//TODO: make this render a nice box around the cell instead of just changing the color
			/*
			if (this->currentRow == row && this->currentCol == column && (getState() == EDITING || getState() == PAUSED))
			{
				SDL_SetRenderDrawColor(mainRenderer, accentColor.r, accentColor.g, accentColor.b, 0xFF);
				SDL_RenderFillRect(mainRenderer, &cellRect);
			}*/
			if (!matrix[row][column])
			{
				totalCount++;
				continue;
			}
			int borderSize = 1;
			if (borderSize > 0)
			{
				cellRect.x += borderSize;
				cellRect.y += borderSize;
				cellRect.w -= borderSize * 2;
				cellRect.h -= borderSize * 2;
			}
			SDL_RenderFillRect(mainRenderer, &cellRect);
			cellCount++;
			totalCount++;
		}
	}
	std::cerr << "Cell Count: " << cellCount << "Total Count" << totalCount << std::endl;
	if (getState() != MENU)
	{
		SDL_SetRenderDrawColor(mainRenderer, accentColor.r, accentColor.g, accentColor.b, 0xFF);
		SDL_Rect boundingBox = {boardPosition.x - 1, boardPosition.y - 1, cellWidth * board->getWidth() + 2, cellHeight * board->getHeight() + 2};
		SDL_RenderDrawRect(mainRenderer, &boundingBox);
	}
	SDL_SetRenderDrawColor(mainRenderer, bgColor.r, bgColor.g, bgColor.b, 0xFF);
}

void Controller::renderBoard()
{
	renderBoard(&boardPanel);
}

void Controller::derenderCursor()
{
	//color assumed to be background color already
	int y = boardPanel.y + cellHeight * currentRow + boardPosition.y;
	int x = boardPanel.x + cellWidth * currentCol + boardPosition.x;
	//special status panel case
	if (y + cellHeight > (boardPanel.y + boardPanel.h))
		return;
	SDL_Rect cursorRect = {x, y, cellWidth, cellHeight};
	SDL_RenderDrawRect(mainRenderer, &cursorRect);
}

void Controller::renderCursor()
{
	int y = boardPanel.y + cellHeight * currentRow + boardPosition.y;
	int x = boardPanel.x + cellWidth * currentCol + boardPosition.x;
	SDL_Rect cursorRect = {x, y, cellWidth, cellHeight};
	if (y + cellHeight > (boardPanel.y + boardPanel.h))
		return;

	SDL_SetRenderDrawColor(mainRenderer, accentColor.r, accentColor.g, accentColor.b, 0xFF);
	SDL_RenderDrawRect(mainRenderer, &cursorRect);
	SDL_SetRenderDrawColor(mainRenderer, bgColor.r, bgColor.g, bgColor.b, 0xFF);
	//TODO: make this a thicc box that covers the entire border...
	//do this after updating the borderSize variable in renderBoard()

}

//TODO: rewrite this, make all the text static members, update only the number
void Controller::renderStatusPanel(SDL_Rect * renderArea)
{
	//if we are in the menu, we can hide the status panel
	if (state == MENU)
	{
		return;
	}
	SDL_RenderFillRect(mainRenderer, renderArea);

	std::vector<std::string> stringList;
	std::ostringstream boardSize, status;
	//creating string streams for board size and status
	boardSize << "Size: " << board->getHeight() << "x" << board->getWidth();
	status << "Status: " << getStateName();
	//adding strings to stringList
	stringList.push_back(boardSize.str());
	stringList.push_back(status.str());

	//if we are not editing, we can also add these  details
	if (state != EDITING)
	{
		std::ostringstream iterations, births, deaths, speed;
		iterations << "Iterations: " << board->getIterations();
		births << "Births: " << board->getBirths();
		deaths << "Deaths: " << board->getDeaths();
		speed << "Speed: " << this->speed;
		stringList.push_back(iterations.str());
		stringList.push_back(births.str());
		stringList.push_back(deaths.str());
		stringList.push_back(speed.str());
	}

	//finding an even amount of space along each dimension
	int yShift = renderArea->h / 3;
	int xShift = renderArea->w / 4;

	for (int i = 0; i < stringList.size(); i++)
	{
		int row = i / 3 + 1;
		int col = i % 3 + 1;

		SDL_Surface * surface = TTF_RenderText_Shaded(mainFont, stringList[i].c_str(), mainColor, bgColor);
		//centering each surface onto a point in the 2x3 grid
		SDL_Rect tempRect = {renderArea->x + xShift * col - surface->w/2, renderArea->y + yShift * row - surface->h/2, surface->w, surface->h};
		//creating a texture from surface and rendering it onto tempRect
		SDL_Texture * texture = SDL_CreateTextureFromSurface(mainRenderer, surface);
		SDL_RenderCopy(mainRenderer, texture, NULL, &tempRect);
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);
	}
	SDL_SetRenderDrawColor(mainRenderer, mainColor.r, mainColor.g, mainColor.b, 0xFF);
	SDL_RenderDrawLine(mainRenderer, 0, renderArea->y, renderArea->w, renderArea->y);
	SDL_SetRenderDrawColor(mainRenderer, bgColor.r, bgColor.g, bgColor.b, 0xFF);
}

void Controller::renderStatusPanel()
{
	renderStatusPanel(&statusPanel);
}

void Controller::renderPattern(std::vector<std::vector<bool>>& matrix, SDL_Rect * renderArea)
{
	SDL_SetRenderDrawColor(mainRenderer, accentColor.r, accentColor.g, accentColor.b, 0x7F);
	for (int row = 0; row < matrix.size(); row++)
	{
		for (int column = 0; column < matrix[0].size(); column++)
		{
			int y = renderArea->y + cellHeight * ( (row + currentRow)  % board->getHeight() ) + boardPosition.y;
			int x = renderArea->x + cellWidth * ( (column + currentCol) % board->getWidth() ) + boardPosition.x;
			//TODO: add wraparound stuff
			SDL_Rect cellRect = {x, y, cellWidth, cellHeight};


			//TODO: update this
			bool filled = false;
			if (!filled)
			{
				cellRect.x++;
				cellRect.y++;
				cellRect.w -= 2;
				cellRect.h -= 2;
			}

			if (matrix[row][column])
			{
				SDL_RenderFillRect(mainRenderer, &cellRect);
			}
		}
	}
	SDL_Rect boundingBox = {renderArea->x + cellWidth * currentCol + boardPosition.x, \
		renderArea->y + cellHeight * (currentRow) + boardPosition.y, \
		(int)matrix[0].size() * cellWidth, \
		(int)matrix.size() * cellHeight};
	SDL_RenderDrawRect(mainRenderer, &boundingBox);

	SDL_Rect wrappedBox = {renderArea->x + cellWidth * currentCol + boardPosition.x, \
		renderArea->y + cellHeight * (currentRow) + boardPosition.y, \
		(int)matrix[0].size() * cellWidth, \
		(int)matrix.size() * cellHeight};

	//TODO: fix wrappedBox
	/*
	if (currentCol + matrix[0].size() > board->getWidth())
	{
		std::cout << "X out of bounds, let's make a box...\n";
		wrappedBox.x = renderArea->x + boardPosition.x + board->getWidth();
		wrappedBox.w = renderArea->w = (currentCol + (int)matrix[0].size() - board->getWidth() ) * cellWidth;
	}
	else if (false)
	{
		wrappedBox.x = renderArea->x + boardPosition.x + board->getWidth() *;
	}
	if (currentRow + matrix.size() > board->getHeight())
	{
		std::cout << "Y out of bounds, let's make a box...\n";
		wrappedBox.y = renderArea->y + boardPosition.y
		wrappedBox.h =

			SDL_Rect wrappedBox = {renderArea->x + boardPosition.x, \
				renderArea->y +boardPosition.y, \
				(currentCol + (int)matrix[0].size() - board->getWidth() ) * cellWidth, \
				(currentRow + (int)matrix.size() - board->getHeight()) * cellHeight};
			SDL_RenderDrawRect(mainRenderer, &boundingBox);
	}
	if (c)
	SDL_RenderDrawRect(mainRenderer, &wrappedBox);
	else
		{
			std::cout << "Y out of bounds, let's make a box...\n";
			SDL_Rect wrappedBox = {renderArea->x + boardPosition.x, \
				 renderArea->y + cellHeight * (currentRow) + boardPosition.y, \
				 (currentCol + (int)matrix[0].size() - board->getWidth()) * cellWidth, \
				 (int)matrix.size() * cellHeight};
			SDL_RenderDrawRect(mainRenderer, &boundingBox);
		}

	}
	else if (currentRow + matrix.size() > board->getHeight())
	{
		SDL_Rect wrappedBox = {renderArea->x + cellWidth * currentCol + boardPosition.x, \
			renderArea->y + boardPosition.y, \
			(int)matrix[0].size() * cellWidth, \
			(currentRow + (int)matrix.size() - board->getHeight()) * cellHeight};
		SDL_RenderDrawRect(mainRenderer, &boundingBox);
	}
	*/
	SDL_SetRenderDrawColor(mainRenderer, bgColor.r, bgColor.g, bgColor.b, 0xFF);
}

/*A sub control loop, allows the user the manually toggle cells and add patterns
*/
//TODO:add better exit(0) (i.e. actually add a method)
void Controller::pausedMode()
{
	int x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	derenderCursor();
	updateRC(x, y);
	bool doRenderUpdate = true;
	bool doCursorUpdate = true;
	bool doPan = false;
	//TODO: enable clicking and dragging cells
	//bool placeCell;
    while(getState() == PAUSED)
    {
		while (SDL_PollEvent(&event) != 0)
		{

			switch(this->event.type)
			{
				case SDL_QUIT:
					std::cerr << "exiting...\n";
					setState(EXITING);
					exit(-1); //TODO: exit more gracefully
					break;

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						//TODO: drag mode
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = false;
					}
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doCursorUpdate = true;
					doRenderUpdate = true;
					break;

				case SDL_MOUSEMOTION:
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doCursorUpdate = true;
					break;

				case SDL_MOUSEBUTTONDOWN:
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doCursorUpdate = true;
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						this->board->toggle(this->currentRow, this->currentCol);
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = true;
					}
					doRenderUpdate = true;
					break;

				case SDL_MOUSEWHEEL:
					derenderCursor();
					updateRC(x, y);
					setZoom(event.wheel.y);
					doCursorUpdate = true;
					doRenderUpdate = true;
					break;

				case SDL_KEYDOWN:
				switch(this->event.key.keysym.sym)
				{
					case SDLK_UP:
						derenderCursor();
						this->currentRow--;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentRow < (boardPanel.y - boardPosition.y) / cellHeight + 1)
							doRenderUpdate = true;
						break;

					case SDLK_DOWN:
						derenderCursor();
						this->currentRow++;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentRow > (boardPanel.y + boardPanel.h - boardPosition.y) / cellHeight - 2)
							doRenderUpdate = true;
						break;

					case SDLK_LEFT:
						derenderCursor();
						this->currentCol--;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentCol < (boardPanel.x - boardPosition.x) / cellWidth + 1)
							doRenderUpdate = true;
						break;

					case SDLK_RIGHT:
						derenderCursor();
						this->currentCol++;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentCol > (boardPanel.x + boardPanel.w - boardPosition.x) / cellWidth - 2)
							doRenderUpdate = true;
						break;

					case SDLK_SPACE:
						this->board->toggle(this->currentRow, this->currentCol);
						doRenderUpdate = true;
						break;

					case SDLK_KP_ENTER:
					case SDLK_RETURN:
						runIteration();
						doRenderUpdate = true;
						break;

					case SDLK_RIGHTBRACKET:
						setSpeed(1);
						break;

					case SDLK_LEFTBRACKET:
						setSpeed(-1);
						break;

					case SDLK_KP_PLUS:
					case SDLK_PLUS:
					case SDLK_EQUALS:
						derenderCursor();
						setZoom(1);
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_KP_MINUS:
					case SDLK_MINUS:
						derenderCursor();
						setZoom(-1);
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_r:
						derenderCursor();
						resetZoom();
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_h:
						getKeybindingsBox();
						break;

					case SDLK_a:
					{
						std::string patternFilename = getStringInput("Enter pattern name.");
						if (patternFilename == "")
							break;
						Pattern * pattern = nullptr;
						try
						{
							pattern = new Pattern("saved/" + patternFilename);
							if (pattern->getHeight() > board->getHeight() || pattern->getWidth() > board->getWidth())
							{
								throw "Pattern is too big!";
							}
						}
						catch (char const* message)
						{
							getConfirmationBox(message);
							continue;
						}
						pattern->printBoard();
						state = PLACE;
						placeMode(*pattern);
						delete pattern;
						break;
					}
					case SDLK_BACKQUOTE:
						std::cerr << currentRow << "," << currentCol << std::endl;
						break;
					case SDLK_p:
						setState(RUNNING);
						break;
					case SDLK_ESCAPE:
						saveCurrent();
						setState(MENU);
				}
	        }
		}

		if (doPan)
		{
			setPan(x,y);
			setZoom(0);
		}
		if (doRenderUpdate)
		{
			clearScreen();
			renderBoard();
			renderStatusPanel();
		}
		if (doRenderUpdate || doCursorUpdate)
		{
			renderCursor();
			updateScreen();
		}
		doRenderUpdate = false;
		doCursorUpdate = false;
	}
}

void Controller::runningMode()
{
	int x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	derenderCursor();
	updateRC(x, y);
	bool doPan = false;
    while(getState() == RUNNING)
    {
		while (SDL_PollEvent(&event) != 0)
		{
			switch(this->event.type)
			{
				case SDL_QUIT:
					std::cerr << "exiting...\n";
					setState(EXITING);
					exit(-1); //TODO: exit more gracefully
					break;

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = false;
					}
					//flow into mouse motion case
				case SDL_MOUSEMOTION:
					SDL_GetMouseState(&x, &y);
					updateRC(x, y);
					break;

				case SDL_MOUSEBUTTONDOWN:
					SDL_GetMouseState(&x, &y);
					updateRC(x, y);
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = true;
					}
					break;

				case SDL_MOUSEWHEEL:
					setZoom(event.wheel.y);

				case SDL_KEYDOWN:
				switch(this->event.key.keysym.sym)
				{
					case SDLK_UP:
						this->currentRow--;
						checkRC();
						break;

					case SDLK_DOWN:
						this->currentRow++;
						checkRC();
						break;

					case SDLK_LEFT:
						this->currentCol--;
						checkRC();
						break;

					case SDLK_RIGHT:
						this->currentCol++;
						checkRC();
						break;

					case SDLK_RIGHTBRACKET:
						setSpeed(1);
						break;

					case SDLK_LEFTBRACKET:
						setSpeed(-1);
						break;

					case SDLK_KP_PLUS:
					case SDLK_PLUS:
					case SDLK_EQUALS:
						setZoom(1);
						break;

					case SDLK_KP_MINUS:
					case SDLK_MINUS:
						setZoom(-1);
						break;

					case SDLK_r:
						resetZoom();
						break;

					case SDLK_h:
						getKeybindingsBox();
						break;

					case SDLK_p:
						setState(PAUSED);
						break;

					case SDLK_ESCAPE:
						saveCurrent();
						setState(MENU);
				}
	        }
		}

		if (doPan)
		{
			setPan(x,y);
			setZoom(0);
		}
		//TODO: FIX THIS WITH SDL TIMING
		clearScreen();
		SDL_Delay(1.0/getSpeed() * 1000);
		for (int i = 1; (getSpeed () > 1000) && (i < getSpeed() / 1000); i++)
		{
			runIteration();
		}
		runIteration();
		renderBoard();
		renderStatusPanel();
		updateScreen();
	}
}

void Controller::placeMode(Pattern pattern)
{
	derenderCursor();
	std::cerr << "place mode entered\n";
	int x, y;
	bool doPan = false;
	bool doRenderUpdate = true;
	pattern.printBoard();
	while (state == PLACE)
	{
		while (SDL_PollEvent(&event) != 0)
		{
		    switch(this->event.type)
			{
				case SDL_QUIT:
				setState(EXITING);
				std::cerr << "exiting...\n";
				exit(-1); //TODO: exit more gracefully
				break;

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = false;
					}
				case SDL_MOUSEMOTION:
				SDL_GetMouseState(&x, &y);
				derenderCursor();
				updateRC(x, y);
				break;

				case SDL_MOUSEBUTTONDOWN:
				SDL_GetMouseState(&x, &y);
				derenderCursor();
				updateRC(x, y);
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						board->addPattern(pattern.getMatrix(), currentRow, currentCol);
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = true;
					}
				break;

				case SDL_MOUSEWHEEL:
					setZoom(event.wheel.y);

				case SDL_KEYDOWN:
					switch(this->event.key.keysym.sym)
					{
						case SDLK_UP:
							derenderCursor();
							this->currentRow--;
							checkRC();
							break;

						case SDLK_DOWN:
							derenderCursor();
							this->currentRow++;
							checkRC();
							break;

						case SDLK_LEFT:
							this->currentCol--;
							checkRC();
							break;

						case SDLK_RIGHT:
							derenderCursor();
							this->currentCol++;
							checkRC();
							break;

						case SDLK_KP_ENTER:
						case SDLK_RETURN:
						case SDLK_SPACE:
							board->addPattern(pattern.getMatrix(), currentRow, currentCol);
							break;

						case SDLK_RIGHTBRACKET:
							pattern.rotate();
							pattern.rotate();
							pattern.rotate();
							break;

						case SDLK_LEFTBRACKET:
							pattern.rotate();
							break;

						case SDLK_KP_PLUS:
						case SDLK_PLUS:
						case SDLK_EQUALS:
							setZoom(1);
							break;

						case SDLK_KP_MINUS:
						case SDLK_MINUS:
							setZoom(-1);
							break;

						case SDLK_r:
							resetZoom();
							break;

						case SDLK_h:
							getKeybindingsBox();
							break;

						case SDLK_a:
						case SDLK_ESCAPE:
							setState(PAUSED);
							break;
				}
	        }
		}
		//TODO add a derenderpattern?
		if (doPan)
		{
			setPan(x, y);
			setZoom(0);
		}
		if (doRenderUpdate)
		{
			clearScreen();
			renderBoard(&boardPanel);
			renderPattern(pattern.getMatrix(), &boardPanel);
			renderStatusPanel(&statusPanel);
			updateScreen();
		}
	}
}

void Controller::editorMode()
{
	int x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	derenderCursor();
	updateRC(x, y);
	bool doRenderUpdate = true;
	bool doCursorUpdate = true;
	bool doPan = false;
	//TODO: enable clicking and dragging cells
	//bool placeCell;
    while(getState() == EDITING)
    {
		while (SDL_PollEvent(&event) != 0)
		{

			switch(this->event.type)
			{
				case SDL_QUIT:
					std::cerr << "exiting...\n";
					setState(EXITING);
					exit(-1); //TODO: exit more gracefully
					break;

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						//TODO: drag mode
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = false;
					}
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doCursorUpdate = true;
					break;
				case SDL_MOUSEMOTION:
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doCursorUpdate = true;
					break;

				case SDL_MOUSEBUTTONDOWN:
					SDL_GetMouseState(&x, &y);
					derenderCursor();
					updateRC(x, y);
					doRenderUpdate = true;
					doCursorUpdate = true;
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						this->board->toggle(this->currentRow, this->currentCol);
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						doPan = true;
					}
					break;

				case SDL_MOUSEWHEEL:
					derenderCursor();
					updateRC(x, y);
					setZoom(event.wheel.y);
					doCursorUpdate = true;
					doRenderUpdate = true;
					break;

				case SDL_KEYDOWN:
				switch(this->event.key.keysym.sym)
				{
					case SDLK_UP:
						derenderCursor();
						this->currentRow--;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentRow < (boardPanel.y - boardPosition.y) / cellHeight + 1)
							doRenderUpdate = true;
						break;

					case SDLK_DOWN:
						derenderCursor();
						this->currentRow++;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentRow > (boardPanel.y + boardPanel.h - boardPosition.y) / cellHeight - 2)
							doRenderUpdate = true;
						break;

					case SDLK_LEFT:
						derenderCursor();
						this->currentCol--;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentCol < (boardPanel.x - boardPosition.x) / cellWidth + 1)
							doRenderUpdate = true;
						break;

					case SDLK_RIGHT:
						derenderCursor();
						this->currentCol++;
						checkRC();
						doCursorUpdate = true;
						//check that the cursor is in range, if not, re-render
						if (currentCol > (boardPanel.x + boardPanel.w - boardPosition.x) / cellWidth - 2)
							doRenderUpdate = true;
						break;

					case SDLK_SPACE:
						this->board->toggle(this->currentRow, this->currentCol);
						doRenderUpdate = true;
						break;

					case SDLK_KP_PLUS:
					case SDLK_PLUS:
					case SDLK_EQUALS:
						derenderCursor();
						setZoom(1);
						renderCursor();
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_KP_MINUS:
					case SDLK_MINUS:
						derenderCursor();
						setZoom(-1);
						renderCursor();
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_r:
						derenderCursor();
						resetZoom();
						renderCursor();
						doCursorUpdate = true;
						doRenderUpdate = true;
						break;

					case SDLK_h:
						getKeybindingsBox();
						break;

					case SDLK_a:
					{
						std::string patternFilename = getStringInput("Enter pattern name.");
						if (patternFilename == "")
							break;
						Pattern * pattern = nullptr;
						try
						{
							pattern = new Pattern("patterns/" + patternFilename);
							if (pattern->getHeight() > board->getHeight() || pattern->getWidth() > board->getWidth())
							{
								throw "Pattern is too big!";
							}
						}
						catch (char const* message)
						{
							getConfirmationBox(message);
							continue;
						}
						pattern->printBoard();
						state = PLACE;
						placeMode(*pattern);
						delete pattern;
						break;
					}

					case SDLK_ESCAPE:
						saveCurrent();
						setState(MENU);
				}
	        }
		}

		if (doPan)
		{
			setPan(x,y);
			setZoom(0);
		}
		if (doRenderUpdate)
		{
			clearScreen();
			renderBoard();
			renderStatusPanel();
		}
		if (doCursorUpdate || doRenderUpdate)
		{
			renderCursor();
			updateScreen();
		}
		doCursorUpdate = false;
		doRenderUpdate = false;
	}
}
