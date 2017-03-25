void toggle(int &x, int &y)	//make sure the pointer stuff works
{
	if(matrix[y][x] == false)
	{
		matrix[y][x] = true;
	}
	else{
		matrix[y][x] = false;
	}
}

bool **getMatrix()
{
	return matrix;
}

//do we need to include int height/width???????????
void saveState(string fileName, int height, int width)	
{
	ofstream out(fileName);
	out << height << "\n";	//first line tells the program the height of the saved matrix
	out << width << "\n";	//second line tells the program the width of the saved matrix
	
	for(int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			out << matrix[i][j];
		}
		out << "\n";
	}
	out.close();
}

void addPattern(string fileName, int x, int y)
{
	ifstream in;
	in.open(fileName.c_str());
	
	if(!in.is_open())
	{
		cerr << "File not opened" << endl;
		return 1;
	}
	
	int heightOfSaved = stoi(getline(fileName, line));
	int widthOfSaved;
	string line;
	
	getline(fileName, line);
	//now convert to the heightOfSaved
	getline(fileName, line);
	//now convert this to the widthOfSaved
	
	
}