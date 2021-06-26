package fetch

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/strang1ato/nhi/pkg/sqlite"
)

// Fetch retrieves shell session optionally with given range of commands
func Fetch(tableName, startEndRange string) error {
	db, err := sqlite.OpenDb()
	if err != nil {
		return err
	}

	startEndRange = strings.TrimPrefix(startEndRange, "[")
	startEndRange = strings.TrimSuffix(startEndRange, "]")

	sliceStartEndRange := strings.SplitN(startEndRange, ":", 2)

	billion := 1000000000
	if sliceStartEndRange[0] == "" {
		sliceStartEndRange[0] = "0"
	}
	if sliceStartEndRange[1] == "" {
		sliceStartEndRange[1] = strconv.Itoa(billion - 1)
	}

	intStartRange, err := strconv.Atoi(sliceStartEndRange[0])
	if err != nil {
		return errors.New("Start range is not a number")
	}
	intEndRange, err := strconv.Atoi(sliceStartEndRange[1])
	if err != nil {
		return errors.New("End range is not a number")
	}

	var query string
	if intStartRange < billion && intEndRange < billion {
		if intStartRange < 0 && intEndRange < 0 {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE rowid >= (SELECT max(rowid)+%s FROM `%s`) AND rowid < (SELECT max(rowid)+%s FROM `%s`);",
				tableName, sliceStartEndRange[0], tableName, sliceStartEndRange[1], tableName)
		} else if intStartRange < 0 {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE rowid >= (SELECT max(rowid)+%s FROM `%s`) AND rowid <= %s;",
				tableName, sliceStartEndRange[0], tableName, sliceStartEndRange[1])
		} else if intEndRange < 0 {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE rowid > %s AND rowid < (SELECT max(rowid)+%s FROM `%s`);",
				tableName, sliceStartEndRange[0], sliceStartEndRange[1], tableName)
		} else {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE rowid > %s AND rowid <= %s;",
				tableName, sliceStartEndRange[0], sliceStartEndRange[1])
		}
	} else if intStartRange < billion {
		if intStartRange < 0 {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE rowid >= (SELECT max(rowid)+%s FROM `%s`) AND indicator <= %s;",
				tableName, sliceStartEndRange[0], tableName, sliceStartEndRange[1])
		} else {
			query = fmt.Sprintf("SELECT command, output FROM `%s` WHERE rowid > %s AND indicator <= %s;",
				tableName, sliceStartEndRange[0], sliceStartEndRange[1])
		}
	} else if intEndRange < billion {
		if intEndRange < 0 {
			query = fmt.Sprintf("SELECT command, output FROM `%s`"+
				"WHERE indicator >= %s AND rowid < (SELECT max(rowid)+%s FROM `%s`);",
				tableName, sliceStartEndRange[0], sliceStartEndRange[1], tableName)
		} else {
			query = fmt.Sprintf("SELECT command, output FROM `%s` WHERE indicator >= %s AND rowid <= %s;",
				tableName, sliceStartEndRange[0], sliceStartEndRange[1])
		}
	} else {
		query = fmt.Sprintf("SELECT command, output FROM `%s` WHERE indicator >= %s AND indicator <= %s;",
			tableName, sliceStartEndRange[0], sliceStartEndRange[1])
	}

	rows, err := db.Query(query)
	if err != nil {
		if err.Error() == "no such table: "+tableName {
			return errors.New("no such shell session: " + tableName)
		}
		return err
	}

	var content strings.Builder
	for rows.Next() {
		var command, output string
		rows.Scan(&command, &output)

		if command == "" {
			continue
		}
		content.WriteString("PS1 ")
		content.WriteString(command)
		content.WriteString(output)
	}

	fmt.Print(content.String())
	return nil
}