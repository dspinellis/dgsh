/*-
 *
 * Collect and print Web statistics from common log format data
 * read from the standard input.
 *
 * Copyright 2004-2013 Diomidis Spinellis
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
import java.io.InputStreamReader;
import java.util.Calendar;
import java.util.Comparator;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import javax.management.openmbean.InvalidKeyException;

class WebStats {

    /** Print header with an underline of the same length */
    static void header(String s) {
	System.out.println();
	System.out.println(s);
	for (int i = 0; i < s.length(); i++)
	    System.out.print('-');
	System.out.println();
    }

    /** Add to the map's member the specified value */
    static void add(Map<String, Integer> map, String member, int value) {
	Integer i = map.get(member);
	if (i == null)
	    i = new Integer(value);
	else
	    i = new Integer(i.intValue() + value);
	map.put(member, i);
    }

    /** List the contents of the given map */
    static void list(String title, Map<String, Integer> map) {
	header(title);
	for (Map.Entry e : map.entrySet())
	    System.out.println(e.getValue() + " " + e.getKey());
    }

    /** List the n-top contents of the given map ordered by their values.  */
    static void sortedList(String title, Map<String, Integer> map, int n) {
	header(title);
	// Sort by value
	TreeSet <Map.Entry<String, Integer>> valueOrder
	    = new TreeSet<Map.Entry<String, Integer>>(new
	    Comparator<Map.Entry<String, Integer>>() {
		public int compare(Map.Entry<String, Integer> a,
			Map.Entry<String, Integer> b) {
		    return (-a.getValue().compareTo(b.getValue()));
		}
	    }
	);
	valueOrder.addAll(map.entrySet());

	// Print top n entries
	int i = 0;
	for (Map.Entry e : valueOrder) {
	    System.out.println(e.getValue() + " " + e.getKey());
	    if (++i == n)
		break;
	}
    }

    public static void main(String args[]) {

	// Compile regular expressions
	Pattern logLinePattern = null;
	Pattern areaPattern = null;
	Pattern numericIPPattern = null;
	Pattern domainPattern = null;
	Pattern topDomainPattern = null;
	try {
	    // A standard log line is a line like:
	    // 192.168.136.16 - - [26/Jan/2004:19:45:48 +0200] "GET /c136.html HTTP/1.1" 200 1674
	    logLinePattern = Pattern.compile(
	    "(?<host>[-\\w.]+)\\s+" +	// Host
	    "([-\\w]+)\\s+" +		// Logname
	    "([-\\w]+)\\s+" +		// User
	    "\\[(?<day>\\d+)/" +	// Day of month
	    "(?<month>\\w+)/" +		// Month
	    "(?<year>\\d+):" +		// Year
	    "(?<hour>\\d+):" +		// Hour
	    "(?<min>\\d+)" +		// Minute
	    "([^]]+?)\\]\\s+" +		// Rest of time
	    "\"([-\\w]+)\\s*" +		// Request verb
	    "(?<url>[^\\s]*)" +		// Request URL
	    "([^\"]*?)\"\\s+" +		// Request protocol etc.
	    "(\\d+)\\s+" +		// Status
	    "(?<bytes>[-\\d]+)"		// Bytes
	    );

	    // The first part of a request
	    areaPattern = Pattern.compile("^/?([^/]+)");

	    // Host names
	    numericIPPattern = Pattern.compile("\\.\\d+$");
	    domainPattern = Pattern.compile("[^.]\\.(.*)");
	    topDomainPattern = Pattern.compile(".*\\.(.*)");
	} catch (PatternSyntaxException e) {
	    System.err.println("Invalid RE syntax: " + e);
	    System.exit(1);
	}

	// A Gregorian calendar, used for converting dates to day of week
	GregorianCalendar cal = new GregorianCalendar();

	// Map from month name to month value
	Locale english = new Locale("EN");
	Map<String, Integer> monthValue = cal.getDisplayNames(Calendar.MONTH, Calendar.SHORT, english);

	// Data structures for gathering the results
	HashMap<String, Integer> hostCount = new HashMap<String, Integer>();
	HashMap<String, Integer> hourCount = new HashMap<String, Integer>();
	HashMap<String, Integer> requestCount = new HashMap<String, Integer>();
	HashMap<String, Integer> areaCount = new HashMap<String, Integer>();
	HashMap<String, Integer> transferVolume = new HashMap<String, Integer>();
	HashMap<String, Integer> topDomainCount = new HashMap<String, Integer>();
	HashMap<String, Integer> domainCount = new HashMap<String, Integer>();
	TreeMap<String, Integer> dateCount = new TreeMap<String, Integer>();
	HashMap<String, Integer> dowCount = new HashMap<String, Integer>();

	long accessCount = 0;
	long byteCount = 0;
	long logByteCount = 0;

	BufferedReader in = new BufferedReader(new InputStreamReader(System.in));

	try {
	    String line;
	    while ((line = in.readLine()) != null) {

		logByteCount += line.length() + 1;
		accessCount++;

		Matcher logLineMatched = logLinePattern.matcher(line);
		if (!logLineMatched.matches())
		    continue;

		add(hourCount, logLineMatched.group("hour"), 1);

		// Remote host
		String host = logLineMatched.group("host");
		add(hostCount, host, 1);

		// Transfer bytes by remote host
		try {
		    int bytes = Integer.parseInt(logLineMatched.group("bytes"));
		    byteCount += bytes;
		    add(transferVolume, host, bytes);
		} catch (NumberFormatException e) {
		}

		// Remote domains and top domains
		Matcher numericIPMatched = numericIPPattern.matcher(host);
		if (!numericIPMatched.find()) {
		    Matcher topDomainMatched = topDomainPattern.matcher(host);
		    if (topDomainMatched.find())
			add(topDomainCount, topDomainMatched.group(1), 1);

		    Matcher domainMatched = domainPattern.matcher(host);
		    if (domainMatched.find())
			add(domainCount, domainMatched.group(1), 1);
		}

		// Request
		String url = logLineMatched.group("url");
		add(requestCount, url, 1);

		// Area part of the URL
		Matcher areaMatched = areaPattern.matcher(url);
		if (areaMatched.find())
		    add(areaCount, areaMatched.group(1), 1);

		// Date
		String date = logLineMatched.group("day") + "/" +
		    logLineMatched.group("month") + "/" +
		    logLineMatched.group("year");
		add(dateCount, date, 1);

		// Day of week
		try {
		    Integer month = monthValue.get(logLineMatched.group("month"));
		    if (month == null)
			throw new InvalidKeyException();
		    int day = Integer.parseInt(logLineMatched.group("day"));
		    int year = Integer.parseInt(logLineMatched.group("year"));
		    cal.set(year, month, day);
		    cal.get(Calendar.DAY_OF_WEEK);
		    String dayOfWeek = cal.getDisplayName(Calendar.DAY_OF_WEEK, Calendar.SHORT, english);
		    add(dowCount, dayOfWeek, 1);
		} catch (NumberFormatException | InvalidKeyException e) {
		    ;
		}
	    }
	} catch (Exception e) {
	    System.err.println("Error reading line: " + e);
	    System.exit(1);
	}

	System.out.println("			WWW server statistics");
	System.out.println("			=====================");

	header("Summary");
	System.out.println("Number of accesses: " + accessCount);
	System.out.println("Number of Gbytes transferred: " + byteCount / 1024 / 1024 / 1024);
	System.out.println("Number of hosts: " + hostCount.size());
	System.out.println("Number of domains: " + domainCount.size());
	System.out.println("Number of top level domains: " + topDomainCount.size());
	System.out.println("Number of different pages: " + requestCount.size());
	System.out.println("Accesses per day: " + accessCount / dateCount.size());
	System.out.println("Mbytes per day: " + byteCount / dateCount.size() / 1024 / 1024);
	System.out.println("Mbytes log file size: " + logByteCount / 1024 / 1024);

	sortedList("Top 20 Requests", requestCount, 20);
	sortedList("Top 20 Area Requests", areaCount, 20);
	sortedList("Top 10 Hosts", hostCount, 10);
	sortedList("Top 10 Hosts by Transfer", transferVolume, 10);
	sortedList("Top 10 Domains", domainCount, 10);
	sortedList("Top 20 Top Level Domain Accesses", topDomainCount, 20);
	sortedList("Accesses by Day of Week", dowCount, -1);
	sortedList("Accesses by Local Hour", hourCount, -1);
	list("Accesses by Date", dateCount);
    }
}
