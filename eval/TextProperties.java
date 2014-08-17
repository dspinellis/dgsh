/*-
 *
 * Collect and print text frequency statistics from the text
 * read from the standard input.
 *
 * Copyright 2014 Diomidis Spinellis
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;

class TextProperties {

    /** Increment the map's member by one */
    static void increment(Map<String, Integer> map, String member) {
	Integer i = map.get(member);
	if (i == null)
	    i = new Integer(1);
	else
	    i = new Integer(i.intValue() + 1);
	map.put(member, i);
    }

    /** Increment in the map all n-grams in the specified word */
    static void ngramIncrement(Map<String, Integer> map, StringBuffer word, int n) {
	for (int i = 0; i <= word.length() - n; i++)
	    increment(map, word.substring(i, i + n));
    }

    /** Save the contents of the given map ordered by their values.  */
    static void sortedList(Map<String, Integer> map, String fileName) {
	// Sort by value
	TreeSet <Map.Entry<String, Integer>> valueOrder
	    = new TreeSet<Map.Entry<String, Integer>>(new
	    Comparator<Map.Entry<String, Integer>>() {
		public int compare(Map.Entry<String, Integer> a,
			Map.Entry<String, Integer> b) {
		    int c = -a.getValue().compareTo(b.getValue());
		    if (c != 0)
			return (c);
		    else
			return -a.getKey().compareTo(b.getKey());
		}
	    }
	);
	valueOrder.addAll(map.entrySet());

	try {
	    PrintWriter out = new PrintWriter(new BufferedWriter(new
			OutputStreamWriter(new FileOutputStream(fileName))));

	    // Print entries
	    for (Map.Entry e : valueOrder)
		out.println(e.getValue() + " " + e.getKey());
	    out.close();
	} catch (Exception e) {
	    System.err.println("Error writing to file: " + e);
	    System.exit(1);
	}
    }

    public static void main(String args[]) {
	// Data structures for gathering the results
	HashMap<String, Integer> countCharacter = new HashMap<String, Integer>();
	HashMap<String, Integer> countDigram = new HashMap<String, Integer>();
	HashMap<String, Integer> countTrigram = new HashMap<String, Integer>();
	HashMap<String, Integer> countWord = new HashMap<String, Integer>();

	BufferedReader in = new BufferedReader(new InputStreamReader(System.in));

	try {
	    int c;
	    StringBuffer word = new StringBuffer();

	    while ((c = System.in.read()) != -1) {
		if (Character.isLetter(c))
		    word.append((char)c);
		else {
		    if (word.length() > 0) {
			increment(countWord, word.toString());
			ngramIncrement(countCharacter, word, 1);
			ngramIncrement(countDigram, word, 2);
			ngramIncrement(countTrigram, word, 3);
			word.setLength(0);
		    }
		}
	    }

	} catch (Exception e) {
	    System.err.println("Error reading character: " + e);
	    System.exit(1);
	}
	sortedList(countWord, "words.txt");
	sortedList(countCharacter, "character.txt");
	sortedList(countDigram, "digram.txt");
	sortedList(countTrigram, "trigram.txt");
    }
}
