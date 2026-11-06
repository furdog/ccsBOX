#!/usr/bin/awk -f

# This macro replaces all occurrences of @ENV:var
# with the content of the environment variable.
{
	# Search for the pattern @ENV:var in each line
	while (match($0, /@ENV:[a-zA-Z_][a-zA-Z0-9_]*/)) {
		# Extract the variable name
		var_name = substr($0, RSTART+5, RLENGTH-5)

		# Get the value of the environment variable
		var_value = ENVIRON[var_name]

		# Replace the pattern @ENV:var with the variable's value
		$0 = substr($0, 1, RSTART-1) var_value \
		     substr($0, RSTART+RLENGTH)
	}

	# Output the modified line
	print
}
