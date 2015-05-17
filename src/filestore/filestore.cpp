
#include <iostream>
#include <string>
#include <stdio.h> //rename

#include <collector.h>
#include <utils.h> //set_union(), set_intersect(), dir_exists(), PATH_SEP
#include <filestore/types.h>
#include <filestore/filestore.h>



FileStore::FileStore()
{
	//run command to return /n delimited list of files in the current directory
	FILE* pipe = popen(config->find_cmd.c_str(), "r");

	if(!pipe)
		return;

	char *line = NULL;
	size_t size = 0;
	size_t root_path_length = config->cwd_path.length() + 1;

	while(!feof(pipe))
	{
		if(getline(&line, &size, pipe) != -1)
		{
			std::string path = std::string(line);
			
			//pop the ending newline
			path.pop_back();

			//remove root working directory
			path = path.substr(root_path_length, std::string::npos);

			//create a new File object, and save it in the vector
			insert_file(new File(path));
		}
	}

	pclose(pipe);
}


FileStore::~FileStore()
{
	for(File* file: files)
		delete file;

	for(auto e: tags)
		delete e.second; //the Tag_Entry

	files.clear();
	tags.clear();
}

//turns Selectors into Selections
Selection* FileStore::select(Selector* selector)
{
	//result holders
	file_set r_files;
	entry_set r_subtags;

	if(!selector->is_empty())
	{
		/*
			Intersections
		*/

		bool first = true;
		for(std::string tag: selector->get_tag_intersections())
		{
			//prevent unknown tags from destroying the query
			if(!has_tag(tag))
				continue;

			Tag_Entry* entry = tags[tag];

			if(first)
			{
				//the first tag to be processed is a subset of the universe
				r_files = entry->files;
				first = false;
			}
			else
			{
				file_set r_files_intersect;
				set_intersect<file_set>(r_files_intersect,
										r_files,
										entry->files);

				//if the intersection created a null set, disregard this tag
				if(r_files_intersect.size() > 0)
				{
					r_files = r_files_intersect;
				}
			}
		}

		/*
			Exclusions
		*/

		for(std::string tag: selector->get_tag_exclusions())
		{
			//prevent unknown tags from destroying the query
			if(!has_tag(tag))
				continue;

			Tag_Entry* entry = tags[tag];

			//exclude these files from the selection
			for(File* f: entry->files)
				r_files.erase(f);
		}

		/*
			Manual Includes & Excludes
		*/

		file_map_bool inexclude = selector->get_inexclude();
		for(auto e: inexclude)
		{
			if(e.second)
				r_files.insert(e.first); //include this file
			else
				r_files.erase(e.first); //exclude this file
		}

		/*
			Subtags
		*/

		for(File* file: r_files)
		{
			set_union(r_subtags, file->tags);
		}
	}

	//done selecting
	delete selector;
	return new Selection(&files, r_files, r_subtags);
}


void FileStore::operation(Selection* selection, Operation* operation)
{
	switch(operation->get_op())
	{
		case ADD_TAG:
			add_tag(selection, operation->get_tag());
			break;

		case REMOVE_TAG:
			remove_tag(selection, operation->get_tag());
			break;

		case DELETE_FILES:
			break;
	}
}


bool FileStore::has_tag(const std::string & tag)
{
	return (tags.find(tag) != tags.end());
}

void FileStore::insert_file(File* file)
{
	files.push_back(file);

	//get all tags, relative to the current working directory
	tag_set file_tags = file->compute_tags();

	//first iteration, populate the tag map with any new tags
	for(std::string tag: file_tags)
	{
		Tag_Entry* entry = NULL;

		if(!has_tag(tag))
		{
			//create a new entry object for this tag
			entry = new Tag_Entry;
			entry->tag = tag;
			entry->files.insert(file);

			tags[tag] = entry;
		}
		else
		{
			//add the file to the correct tag file_set
			entry = tags[tag];
			entry->files.insert(file);
		}

		//give the file a pointer to each of its Tag_Entry
		file->tags.insert(entry);
	}
}

void FileStore::add_tag(Selection* selection, const std::string & tag)
{
	//update the data sctructure for the new tag
	Tag_Entry* entry;
	
	if(has_tag(tag))
	{
		entry = tags[tag];
	}
	else
	{
		//create the new Tag_Entry
		entry = new Tag_Entry;
		entry->tag = tag;
		tags[tag] = entry;
	}

	for(File* file: *selection)
	{
		if(!file->has_tag(entry))
			file->add_tag(entry);
	}
}

void FileStore::remove_tag(Selection* selection, const std::string & tag)
{
	if(!has_tag(tag))
		return; //tag has never been seen before. Done.

	Tag_Entry* entry = tags[tag];

	for(File* file: *selection)
	{
		if(file->has_tag(entry))
			file->remove_tag(entry);
	}

	//delete the Tag_Entry if there are no remaining files with that tag
	if(entry->files.size() == 0)
	{
		tags.erase(tag);
		delete entry;
	}
}
